# ex:ts=8 sw=4:
# $OpenBSD: Vstat.pm,v 1.25 2007/05/27 22:04:17 espie Exp $
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
		if (m/^(.*?)\s+on\s+\/.*?\s+type\s+.*?(?:\s+\((.*?)\))?$/) {
			my ($dev, $opts) = ($1, $2);
			my $i = create_device($dev);
			next unless defined $i;
			next unless defined $opts;
			for my $o (split /,\s*/, $opts) {
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
	$giveup = { used => 0 };
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
		if (m/^Filesystem\s+(\d+)\-blocks/) {
			$blocksize = $1;
		} elsif (m/^(.*?)\s+\d+\s+\d+\s+(\-?\d+)\s+\d+\%\s+\/.*?$/) {
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
			my $avail = $data->avail(); 
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
	my $self = $_[0];

	return $self->{avail} - $self->{used}/$self->{blocksize};
}

package OpenBSD::Vstat::Failsafe;
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

# fairly non-descriptive name. Used to store various package information
# during installs and updates.
package OpenBSD::Handle;

use constant {
	NOT_FOUND => 0,
	BAD_PACKAGE => 1,
	CANT_INSTALL => 2,
	ALREADY_INSTALLED => 3
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

package OpenBSD::UpdateSet;
sub new
{
	my $class = shift;
	return bless {}, $class;
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
	require OpenBSD::PackageInfo;
	my @l = ();
	for my $h (@{$self->{older}}) {
		if (OpenBSD::PackageInfo::is_installed($h->{pkgname})) {
			push(@l, $h);
		}
	}
	return @l;
}

sub validate_plists
{
	my ($self, $state) = @_;
	for my $o ($self->older) {
		require OpenBSD::Delete;
		OpenBSD::Delete::validate_plist($o->{plist}, $state);
	}
	for my $n ($self->newer) {
		require OpenBSD::Add;
		OpenBSD::Add::validate_plist($n->{plist}, $state);
	}
	OpenBSD::Vstat::synchronize();
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

1;
