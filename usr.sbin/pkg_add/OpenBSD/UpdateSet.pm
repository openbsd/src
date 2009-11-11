# ex:ts=8 sw=4:
# $OpenBSD: UpdateSet.pm,v 1.21 2009/11/11 12:32:03 espie Exp $
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


# these things don't really live here, they're just stuff that's shared
# between pkg_add and pkg_delete, so to avoid yes another header...

use strict;
use warnings;

package OpenBSD::SharedItemsRecorder;
sub new
{
	my $class = shift;
	return bless {}, $class;
}

sub is_empty
{
	my $self = shift;
	return !(defined $self->{dirs} or defined $self->{users} or
		defined $self->{groups});
}

sub cleanup
{
	my ($self, $state) = @_;
	return if $self->is_empty or $state->{not};

	require OpenBSD::SharedItems;
	OpenBSD::SharedItems::cleanup($self, $state);
}

package OpenBSD::Log;
use OpenBSD::Error;
our @ISA = qw(OpenBSD::Error);

sub set_context
{
	&OpenBSD::Error::set_pkgname;
}

sub dump
{
	&OpenBSD::Error::delayed_output;
}


package OpenBSD::pkg_foo::State;
use OpenBSD::Error;

sub new
{
	my $class = shift;
	my $o = bless {}, $class;
	$o->init(@_);
	return $o;
}

sub init
{
	my $self = shift;
	$self->{l} = OpenBSD::Log->new;
	$self->{progressmeter} = bless {}, "OpenBSD::StubProgress";
}

sub log
{
	my $self = shift;
	if (@_ == 0) {
		return $self->{l};
	} else {
		$self->{l}->print(@_);
	}
}

sub print
{
	my $self = shift;
	$self->progress->print(@_);
}

sub say
{
	my $self = shift;
	$self->progress->print(@_, "\n");
}

sub errprint
{
	my $self = shift;
	$self->progress->errprint(@_);
}

sub errsay
{
	my $self = shift;
	$self->progress->errprint(@_, "\n");
}

sub system
{
	my $self = shift;
	$self->log->system(@_);
}

sub progress
{
	my $self = shift;
	return $self->{progressmeter};
}

# we always have a progressmeter we can print to...
sub setup_progressmeter
{
	my ($self, $opt_x) = @_;
	if (!$opt_x && !$self->{beverbose}) {
		require OpenBSD::ProgressMeter;
		$self->{progressmeter} = OpenBSD::ProgressMeter->new;
	}
}

sub check_root
{
	my $state = shift;
	if ($< && !$state->{defines}->{nonroot}) {
		if ($state->{not}) {
			$state->errsay("$0 should be run as root");
		} else {
			Fatal "$0 must be run as root";
		}
	}
}

sub choose_location
{
	my ($state, $name, $list) = @_;
	if (@$list == 0) {
		$state->say("Can't find $name");
		return undef;
	} elsif (@$list == 1) {
		return $list->[0];
	}

	my %h = map {($_->name, $_)} @$list;
	if ($state->{interactive}) {
		require OpenBSD::Interactive;

		$h{'<None>'} = undef;
		$state->progress->clear;
		my $result = OpenBSD::Interactive::ask_list("Ambiguous: choose package for $name", 1, sort keys %h);
		return $h{$result};
	} else {
		$state->say("Ambiguous: $name could be ", join(' ', keys %h));
		return undef;
	}
}

# stub class when no actual progressmeter that still prints out.
package OpenBSD::StubProgress;
sub clear {}

sub show {}

sub message {}

sub next {}

sub set_header {}

sub print
{
	shift;
	print @_;
}

sub errprint
{
	shift;
	print STDERR @_;
}

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
package OpenBSD::UpdateSet;
sub new
{
	my $class = shift;
	return bless {newer => [], older => {}, hints => []}, $class;
}

sub add_newer
{
	my ($self, @handles) = @_;
	push(@{$self->{newer}}, @handles);
	return $self;
}

sub add_hints
{
	my ($self, @hints) = @_;
	push(@{$self->{hints}}, @hints);
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

sub newer
{
	my $self =shift;
	return @{$self->{newer}};
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
	return map {$_->pkgname} $self->newer;
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
		OpenBSD::Vstat::tally();
	}
	if ($state->{problems}) {
		require OpenBSD::Error;
		OpenBSD::Error::Fatal "fatal issues in ", $self->print;
	}
	OpenBSD::Vstat::synchronize();
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
		for my $p ($set->newer) {
			$self->add_newer($p);
		}
		for my $p ($set->older) {
			$self->add_older($p);
		}
		# BUT XXX tell the tracker we killed the set
		$tracker->remove_set($set);
		# ... and mark it as already done
		$set->{finished} = 1;
	}
	# then regen tracker info for $self
	$tracker->add_set($self);
	return $self;
}

package OpenBSD::PackingList;
sub compute_size
{
	my $plist = shift;
	my $totsize = 0;
	$plist->visit('compute_size', \$totsize);
	$totsize = 1 if $totsize == 0;
	$plist->{totsize} = $totsize;
}

package OpenBSD::PackingElement;
sub mark_progress
{
}

sub compute_size
{
}

package OpenBSD::PackingElement::FileBase;
sub mark_progress
{
	my ($self, $progress, $donesize, $totsize) = @_;
	return unless defined $self->{size};
	$$donesize += $self->{size};
	$progress->show($$donesize, $totsize);
}

sub compute_size
{
	my ($self, $totsize) = @_;

	$$totsize += $self->{size} if defined $self->{size};
}

package OpenBSD::PackingElement::Sample;
sub compute_size
{
	&OpenBSD::PackingElement::FileBase::compute_size;
}

1;
