# ex:ts=8 sw=4:
# $OpenBSD: UpdateSet.pm,v 1.30 2009/11/29 07:54:18 espie Exp $
#
# Copyright (c) 2007 Marc Espie <espie@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.


# an UpdateSet is a list of packages to remove/install.
# it contains three things:
# -> a list of older packages to remove (installed locations)
# -> a list of newer packages to add (might be very simple locations)
# -> a list of "hints", as package names to install
# every add/remove operations manipulate UpdateSet.
#
# Since older packages are always installed, they're organized as a hash.
#
# XXX: an UpdateSet succeeds or fails "together".
# if several packages should be removed/added, then not being able
# to do stuff on ONE of them is enough to invalidate the whole set.
#
# Normal UpdateSets contain one newer package at most.
# Bigger UpdateSets can be created through the merge operation, which
# will be used only when necessary.
use strict;
use warnings;

# hints should behave like locations
package OpenBSD::hint;
sub new
{
	my ($class, $name) = @_;
	bless {name => $name}, $class;
}

sub pkgname
{
	return shift->{name};
}

package OpenBSD::hint2;
our @ISA = qw(OpenBSD::hint);

package OpenBSD::UpdateSet;

sub new
{
	my $class = shift;
	return bless {newer => {}, older => {}, hints => []}, $class;
}

sub cleanup
{
	my ($self, $error) = @_;
	for my $h ($self->older, $self->newer) {
		$h->cleanup($error);
	}
	$self->{error} //= $error;
}

sub has_error
{
	&OpenBSD::Handle::has_error;
}

sub add_newer
{
	my $self = shift;
	for my $h (@_) {
		$self->{newer}->{$h->pkgname} = $h;
	}
	return $self;
}

sub add_older
{
	my $self = shift;
	for my $h (@_) {
		$self->{older}->{$h->pkgname} = $h;
	}
	return $self;
}

sub add_hints
{
	my $self = shift;
	for my $h (@_) {
		push(@{$self->{hints}}, OpenBSD::hint->new($h));
	}
	return $self;
}

sub add_hints2
{
	my $self = shift;
	for my $h (@_) {
		push(@{$self->{hints}}, OpenBSD::hint2->new($h));
	}
	return $self;
}

sub newer
{
	my $self =shift;
	return values %{$self->{newer}};
}

sub older
{
	my $self = shift;
	return values %{$self->{older}};
}

sub hints
{
	my $self =shift;
	return @{$self->{hints}};
}

sub older_names
{
	my $self = shift;
	return keys %{$self->{older}};
}

sub newer_names
{
	my $self =shift;
	return keys %{$self->{newer}};
}

sub hint_names
{
	my $self =shift;
	return map {$_->pkgname} $self->hints;
}

sub older_to_do
{
	my $self = shift;
	# XXX in `combined' updates, some dependencies may remove extra 
	# packages, so we do a double-take on the list of packages we 
	# are actually replacing... for now, until we merge update sets.
	require OpenBSD::PackageInfo;
	my @l = ();
	for my $h ($self->older) {
		if (OpenBSD::PackageInfo::is_installed($h->pkgname)) {
			push(@l, $h);
		}
	}
	return @l;
}

sub print
{
	my $self = shift;
	my @l = ();
	if ($self->newer > 0) {
		push(@l, "installing", $self->newer_names);
	}
	if ($self->older > 0) {
		push(@l, "deinstalling", $self->older_names);
	}
	return join(' ', @l);
}

sub short_print
{
	my $self = shift;
	my @l = ();
	if ($self->older > 0) {
		push(@l, join('+',$self->older_names));
	}
	if ($self->newer > 0) {
		push(@l, join('+', $self->newer_names));
	} elsif ($self->hints > 0) {
		push(@l, join('+', $self->hint_names));
	}
	return join('->', @l);
}

sub shorter_print
{
	my $self = shift;
	return join('+', $self->newer_names);
}

sub validate_plists
{
	my ($self, $state) = @_;
	$state->{problems} = 0;

	for my $o ($self->older_to_do) {
		require OpenBSD::Delete;
		OpenBSD::Delete::validate_plist($o->{plist}, $state);
	}
	$state->{colliding} = [];
	for my $n ($self->newer) {
		require OpenBSD::Add;
		OpenBSD::Add::validate_plist($n->{plist}, $state);
	}
	if (@{$state->{colliding}} > 0) {
		require OpenBSD::CollisionReport;

		OpenBSD::CollisionReport::collision_report($state->{colliding}, $state);
	}
	if (defined $state->{overflow}) {
		$state->vstat->tally;
	}
	if ($state->{problems}) {
		require OpenBSD::Error;
		OpenBSD::Error::Fatal "fatal issues in ", $self->print;
	}
	$state->vstat->synchronize;
}

sub compute_size
{
	my ($self, $state) = @_;
	for my $h ($self->older_to_do, $self->newer) {
		$h->{totsize} = $h->{plist}->compute_size;
	}
}

sub create_new
{
	my ($class, $pkgname) = @_;
	my $set = $class->new;
	$set->add_newer(OpenBSD::Handle->create_new($pkgname));
	return $set;
}

sub from_location
{
	my ($class, $location) = @_;
	my $set = $class->new;
	$set->add_newer(OpenBSD::Handle->from_location($location));
	return $set;
}

# Merge several updatesets together
sub merge
{
	my ($self, $tracker, @sets) = @_;
	# Apparently simple, just add the missing parts
	for my $set (@sets) {
		$self->add_newer($set->newer);
		$self->add_older($set->older);
		# BUT XXX tell the tracker we killed the set
		$tracker->remove_set($set);
		# ... and mark it as already done
		$set->{finished} = 1;
		# XXX and mark it as merged, for eventual updates
		$set->{merged} = $self;
	}
	# then regen tracker info for $self
	$tracker->add_set($self);
	return $self;
}

sub real_set
{
	my $set = shift;
	while (defined $set->{merged}) {
		$set = $set->{merged};
	}
	return $set;
}

1;
