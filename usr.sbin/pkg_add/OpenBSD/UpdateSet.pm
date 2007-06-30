# ex:ts=8 sw=4:
# $OpenBSD: UpdateSet.pm,v 1.2 2007/06/30 11:38:38 espie Exp $
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

package OpenBSD::pkg_foo::State;
use OpenBSD::Error;
our @ISA=(qw(OpenBSD::Error));

sub progress
{
	my $self = shift;
	return $self->{progressmeter};
}

sub setup_progressmeter
{
	my ($self, $opt_x) = @_;
	if (!$opt_x && !$self->{beverbose}) {
		require OpenBSD::ProgressMeter;
		$self->{progressmeter} = OpenBSD::ProgressMeter->new;
	} else {
		$self->{progressmeter} = bless {}, "OpenBSD::StubProgress";
	}
}

sub check_root
{
	my $state = shift;
	if ($< && !$state->{forced}->{nonroot}) {
		if ($state->{not}) {
			Warn "$0 should be run as root\n";
		} else {
			Fatal "$0 must be run as root";
		}
	}
}

package OpenBSD::StubProgress;
sub clear {}

sub show {}

sub message {}

sub next {}

sub set_header {}

# fairly non-descriptive name. Used to store various package information
# during installs and updates.
package OpenBSD::Handle;

use constant {
	BAD_PACKAGE => 1,
	CANT_INSTALL => 2,
	ALREADY_INSTALLED => 3,
	NOT_FOUND => 4
};

sub new
{
	my $class = shift;
	return bless {}, $class;
}

sub set_error
{
	my ($self, $error) = @_;
	$self->{error} = $error;
}

sub has_error
{
	my ($self, $error) = @_;
	if (!defined $self->{error}) {
		return undef;
	}
	if (defined $error) {
		return $self->{error} eq $error;
	}
	return $self->{error};
}

sub create_old
{

	my ($class, $pkgname, $state) = @_;
	my $self= $class->new;
	$self->{pkgname} = $pkgname;

	require OpenBSD::PackageRepository::Installed;

	my $location = OpenBSD::PackageRepository::Installed->new->find($pkgname, $state->{arch});
	if (!defined $location) {
		$self->set_error(NOT_FOUND);
    	} else {
		$self->{location} = $location;
		my $plist = $location->plist;
		if (!defined $plist) {
			$self->set_error(BAD_PACKAGE);
		} else {
			$self->{plist} = $plist;
		}
	}
	return $self;
}

sub create_new
{
	my ($class, $pkg) = @_;
	my $handle = $class->new;
	$handle->{pkgname} = $pkg;
	$handle->{tweaked} = 0;
	return $handle;
}

sub from_location
{
	my ($class, $location) = @_;
	my $handle = $class->new;
	$handle->{pkgname} = $location->{name};
	$handle->{location} = $location;
	$handle->{tweaked} = 0;
	return $handle;
}

package OpenBSD::UpdateSet;
sub new
{
	my $class = shift;
	return bless {newer => [], older => []}, $class;
}

sub add_newer
{
	my ($self, @handles) = @_;
	push(@{$self->{newer}}, @handles);
}

sub add_older
{
	my ($self, @handles) = @_;
	push(@{$self->{older}}, @handles);
}

sub newer
{
	my $self =shift;
	return @{$self->{newer}};
}

sub older
{
	my $self = shift;
	return @{$self->{older}};
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
		if (OpenBSD::PackageInfo::is_installed($h->{pkgname})) {
			push(@l, $h);
		}
	}
	return @l;
}

sub print
{
	my $self = shift;
	my @l = ();
	if (defined $self->{newer}) {
		push(@l, "installing", map {$_->{pkgname}} $self->newer);
	}
	if (defined $self->{older} && @{$self->{older}} > 0) {
		push(@l, "deinstalling", map {$_->{pkgname}} $self->older);
	}
	return join(' ', @l);
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

# temporary shortcut
sub handle
{
	my $self = shift;
	if (defined $self->{newer}) {
		return $self->{newer}[0];
	} else {
		return undef;
	}
}

# temporary creator
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
