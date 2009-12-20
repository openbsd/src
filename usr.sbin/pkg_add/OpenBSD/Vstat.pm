# ex:ts=8 sw=4:
# $OpenBSD: Vstat.pm,v 1.45 2009/12/20 22:38:45 espie Exp $
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
use OpenBSD::Paths;

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
	open(my $cmd1, "-|", OpenBSD::Paths->mount) or print STDERR "Can't run mount\n";
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

	open(my $cmd2, "-|", OpenBSD::Paths->df, $fname)
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

	if ($state->verbose >= 3 or ++($s->{problems}) < 4) {
		$state->errsay("Error: ", $s->{dev}, 
		    " is read-only ($fname)");
	} elsif ($s->{problems} == 4) {
		$state->errsay("Error: ... more files on ", $s->{dev});
	}
	$state->{problems}++;
}

sub report_overflow
{
	my ($s, $state, $fname) = @_;

	if ($state->verbose >= 3 or ++($s->{problems}) < 4) {
		$state->errsay("Error: ", $s->{dev}, 
		    " is not large enough ($fname)");
	} elsif ($s->{problems} == 4) {
		$state->errsay("Error: ... more files do not fit on ", 
		    $s->{dev});
	}
	$state->{problems}++;
	$state->{overflow} = 1;
}

sub report_noexec
{
	my ($s, $state, $fname) = @_;
	$state->errsay("Error: ", $s->{dev}, " is noexec ($fname)");
	$state->{problems}++;
}

package OpenBSD::Vstat::Failsafe;
our @ISA=(qw(OpenBSD::Vstat::MountPoint));

sub avail
{
	return 1;
}

1;
