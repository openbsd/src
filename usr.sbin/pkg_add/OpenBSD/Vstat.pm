# ex:ts=8 sw=4:
# $OpenBSD: Vstat.pm,v 1.39 2007/06/10 16:05:49 espie Exp $
#
# Copyright (c) 2003-2007 Marc Espie <espie@openbsd.org>
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

# Provides stat and statfs-like functions for package handling.

# allows user to add/remove files.

# uses mount and df directly for now.

use strict;
use warnings;

package OpenBSD::Vstat;
use File::Basename;

my $devinfo = {};
my $devinfo2 = {};
my $virtual = {};
my $giveup;

sub create_device($)
{
	my $dev = shift;
	my $n = $devinfo->{$dev};
	if (!defined $n) {
		$n = { dev => $dev, used => 0, delayed => 0, problems => 0 };
		bless $n, "OpenBSD::Vstat::MountPoint";
		$devinfo->{$dev} = $n;
	}
	return $n;
}

sub init_devices()
{
	delete $ENV{'BLOCKSIZE'};
	open(my $cmd1, "/sbin/mount|") or print STDERR "Can't run mount\n";
	while (<$cmd1>) {
		chomp;
		if (m/^(.*?)\s+on\s+\/.*?\s+type\s+.*?(?:\s+\((.*?)\))?$/o) {
			my ($dev, $opts) = ($1, $2);
			my $i = create_device($dev);
			next unless defined $i;
			next unless defined $opts;
			for my $o (split /\,\s*/o, $opts) {
				if ($o eq 'read-only') {
					$i->{ro} = 1;
				} elsif ($o eq 'nodev') {
					$i->{nodev} = 1;
				} elsif ($o eq 'nosuid') {
					$i->{nosuid} = 1;
				} elsif ($o eq 'noexec') {
					$i->{noexec} = 1;
				}
			}
		} else {
			print STDERR "Can't parse mount line: $_\n";
		}
	}
	close($cmd1) or print STDERR "Error running mount: $!\n";
	$giveup = { used => 0, dev => '???' };
	bless $giveup, "OpenBSD::Vstat::Failsafe";
}

sub ask_df($)
{
	my $fname = shift;
	my $info = $giveup;

	open(my $cmd2, "-|", "/bin/df", $fname)
	    or print STDERR "Can't run df\n";
	my $blocksize = 512;
	while (<$cmd2>) {
		chomp;
		if (m/^Filesystem\s+(\d+)\-blocks/o) {
			$blocksize = $1;
		} elsif (m/^(.*?)\s+\d+\s+\d+\s+(\-?\d+)\s+\d+\%\s+\/.*?$/o) {
			my ($dev, $avail) = ($1, $2);
			$info = $devinfo->{$dev};
			if (!defined $info) {
				$info = create_device($dev);
			}
			$info->{avail} = $avail;
			$info->{blocksize} = $blocksize;
		}
	}

	close($cmd2) or print STDERR "Error running df: $!\n";
	return $info;
}

init_devices();

sub filestat($);

sub filestat($)
{
	my $fname = shift;
	my $dev = (stat $fname)[0];

	if (!defined $dev && $fname ne '/') {
		return filestat(dirname($fname));
	}
	if (!defined $dev) {
		return $giveup;
	} else {
		if (!defined $devinfo2->{$dev}) {
			return $devinfo2->{$dev} = ask_df($fname);
		} else {
			return $devinfo2->{$dev};
		}
	}
}

sub vexists($)
{
	my $name = shift;
	if (defined $virtual->{$name}) {
		return $virtual->{$name};
	} else {
		return -e $name;
	}
}

sub account_for($$)
{
	my ($name, $size) = @_;
	my $e = filestat($name);
	$e->{used} += $size;
	return $e;
}

sub account_later($$)
{
	my ($name, $size) = @_;
	my $e = filestat($name);
	$e->{delayed} += $size;
	return $e;
}

sub synchronize
{
	while (my ($k, $v) = each %$devinfo) {
		$v->{used} += $v->{delayed};
		$v->{delayed} = 0;
	}
}

sub add($$;$)
{
	my ($name, $size, $value) = @_;
	if (defined $value) {
		$virtual->{$name} = $value;
	} else {
		$virtual->{$name} = 1;
	}
	return defined($size) ? account_for($name, $size) : undef;
}

sub remove($$)
{
	my ($name, $size) = @_;
	$virtual->{$name} = 0;
	return defined($size) ? account_later($name, -$size) : undef;
}

sub tally()
{
	while (my ($device, $data) = each %$devinfo) {
		if ($data->{used} != 0) {
			print $device, ": ", $data->{used}, " bytes";
			my $avail = $data->avail; 
			if ($avail < 0) {
				print " (missing ", int(-$avail+1), " blocks)";
			}
			print "\n";
		}
	}
}

package OpenBSD::Vstat::MountPoint;
sub avail
{
	my $self = shift;

	return $self->{avail} - $self->{used}/$self->{blocksize};
}

sub report_ro
{
	my ($s, $state, $fname) = @_;

	if ($state->{very_verbose} or ++($s->{problems}) < 4) {
		print STDERR "Error: ", $s->{dev}, 
		    " is read-only ($fname)\n";
	} elsif ($s->{problems} == 4) {
		print STDERR "Error: ... more files on ", $s->{dev}, "\n";
	}
	$state->{problems}++;
}

sub report_overflow
{
	my ($s, $state, $fname) = @_;

	if ($state->{very_verbose} or ++($s->{problems}) < 4) {
		print STDERR "Error: ", $s->{dev}, 
		    " is not large enough ($fname)\n";
	} elsif ($s->{problems} == 4) {
		print STDERR "Error: ... more files do not fit on ", 
		    $s->{dev}, "\n";
	}
	$state->{problems}++;
	$state->{overflow} = 1;
}

sub report_noexec
{
	my ($s, $state, $fname) = @_;
	print STDERR "Error: ", $s->{dev}, " is noexec ($fname)\n";
	$state->{problems}++;
}

package OpenBSD::Vstat::Failsafe;
our @ISA=(qw(OpenBSD::Vstat::MountPoint));

sub avail
{
	return 1;
}

# Here we stuff things common to pkg_add and pkg_delete which do not warrant
# their own file yet.
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
