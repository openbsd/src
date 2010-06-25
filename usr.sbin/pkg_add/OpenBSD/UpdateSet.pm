# ex:ts=8 sw=4:
# $OpenBSD: UpdateSet.pm,v 1.57 2010/06/25 10:34:03 espie Exp $
#
# Copyright (c) 2007-2010 Marc Espie <espie@openbsd.org>
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
# it contains several things:
# -> a list of older packages to remove (installed locations)
# -> a list of newer packages to add (might be very simple locations)
# -> a list of "hints", as package names to install
# -> a list of packages that are kept throughout an update
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
#
# kept packages are needed after merges, where some dependencies may
# not need updating, and to distinguish from old packages that will be
# removed.
#
# for instance, package installation will check UpdateSets for internal
# dependencies and for conflicts. For that to work, we need kept stuff
#
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
	return bless {newer => {}, older => {}, kept => {}, hints => [], updates => 0},
	    $class;
}

sub path
{
	my $set = shift;

	return $set->{path};
}

sub add_repositories
{
	my ($set, @repos) = @_;

	if (!defined $set->{path}) {
		require OpenBSD::PackageRepositoryList;

		$set->{path} = OpenBSD::PackageRepositoryList->new;
	}
	$set->{path}->add(@repos);
}

sub merge_paths
{
	my ($set, $other) = @_;

	if (defined $other->path) {
		if (!defined $set->path) {
			$set->{path} = $other->path;
		} elsif ($set->{path} ne $other->path) {
			$set->add_path(@{$other->{path}});
		}
	}
}

sub match_locations
{
	my ($set, @spec) = @_;
	my $r = [];
	if (defined $set->{path}) {
		$r = $set->{path}->match_locations(@spec);
	}
	if (@$r == 0) {
		require OpenBSD::PackageLocator;
		$r = OpenBSD::PackageLocator->match_locations(@spec);
	}
	return $r;
}

sub cleanup
{
	my ($self, $error, $errorinfo) = @_;
	for my $h ($self->older, $self->newer) {
		$h->cleanup($error, $errorinfo);
	}
	$self->{error} //= $error;
	$self->{errorinfo} //= $errorinfo;
	delete $self->{solver};
	delete $self->{conflict_cache};
	$self->{finished} = 1;
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
		$self->{updates}++;
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

sub add_kept
{
	my $self = shift;
	for my $h (@_) {
		$self->{kept}->{$h->pkgname} = $h;
	}
	return $self;
}

sub move_kept
{
	my $self = shift;
	for my $h (@_) {
		delete $self->{older}->{$h->pkgname};
		delete $self->{newer}->{$h->pkgname};
		$self->{kept}->{$h->pkgname} = $h;
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
	my $self = shift;
	return values %{$self->{newer}};
}

sub older
{
	my $self = shift;
	return values %{$self->{older}};
}

sub kept
{
	my $self = shift;
	return values %{$self->{kept}};
}

sub hints
{
	my $self = shift;
	return @{$self->{hints}};
}

sub older_names
{
	my $self = shift;
	return keys %{$self->{older}};
}

sub newer_names
{
	my $self = shift;
	return keys %{$self->{newer}};
}

sub kept_names
{
	my $self = shift;
	return keys %{$self->{kept}};
}

sub hint_names
{
	my $self = shift;
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
	my $result = "";
	if ($self->kept > 0) {
		$result = "[".join('+', sort $self->kept_names)."]";
	}
	if ($self->older > 0) {
		$result .= join('+',sort $self->older_names)."->";
	}
	if ($self->newer > 0) {
		$result .= join('+', sort $self->newer_names);
	} elsif ($self->hints > 0) {
		$result .= join('+', sort $self->hint_names);
	}
	return $result;
}

sub short_print
{
	my $self = shift;
	my $result = join('+', sort $self->newer_names);
	if (length $result > 30) {
		return substr($result, 0, 27)."...";
	} else {
		return $result;
	}
}

sub validate_plists
{
	my ($self, $state) = @_;
	$state->{problems} = 0;
	delete $state->{overflow};

	for my $o ($self->older_to_do) {
		require OpenBSD::Delete;
		OpenBSD::Delete::validate_plist($o->{plist}, $state);
	}
	$state->{colliding} = [];
	for my $n ($self->newer) {
		require OpenBSD::Add;
		OpenBSD::Add::validate_plist($n->{plist}, $state, $self);
	}
	if (@{$state->{colliding}} > 0) {
		require OpenBSD::CollisionReport;

		OpenBSD::CollisionReport::collision_report($state->{colliding}, $state);
	}
	if (defined $state->{overflow}) {
		$state->vstat->tally;
		# okay, let's retry the other way around if we haven't yet
		if (!defined $state->{delete_first}) {
			if ($state->defines('deletefirst') ||
			    $state->confirm("Delete older packages first", 0)) {
				$state->{delete_first} = 1;
				$state->vstat->drop_changes;
				return $self->validate_plists($state);
			}
		}
	}
	if ($state->{problems}) {
		$state->vstat->drop_changes;
		return 0;
	} else {
		$state->vstat->synchronize;
		return 1;
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

sub merge_if_exists
{
	my ($self, $k, @extra) = @_;

	my @list = ();
	for my $s (@extra) {
		if ($s ne $self && defined $s->{$k}) {
			push(@list, $s->{$k});
		}
	}
	$self->$k->merge(@list);
}

# Merge several updatesets together
sub merge
{
	my ($self, $tracker, @sets) = @_;

	$self->merge_if_exists('solver', @sets);
	$self->merge_if_exists('conflict_cache', @sets);
	# Apparently simple, just add the missing parts
	for my $set (@sets) {
		next if $set eq $self;
		$self->add_newer($set->newer);
		$self->add_older($set->older);
		$self->add_kept($set->kept);
		$self->merge_paths($set);
		# ... and mark it as already done
		$set->{finished} = 1;
		$tracker->handle_set($set);
		$self->{updates} += $set->{updates};
		$set->{updates} = 0;
		# XXX and mark it as merged, for eventual updates
		$set->{merged} = $self;
		delete $set->{solver};
		delete $set->{conflict_cache};
	}
	# then regen tracker info for $self
	$tracker->todo($self);
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
