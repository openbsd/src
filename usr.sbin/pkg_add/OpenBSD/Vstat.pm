# ex:ts=8 sw=4:
# $OpenBSD: Vstat.pm,v 1.7 2004/08/06 07:51:17 espie Exp $
#
# Copyright (c) 2003-2004 Marc Espie <espie@openbsd.org>
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
use Symbol;

my $dirinfo = {};
my $virtual = {};
my $virtual_dir = {};
my $mnts = [];
my $blocksize = 512;

sub create_mntpoint($)
{
	my $mntpoint = shift;
	my $dev = (stat $mntpoint)[0];
	# check that stat worked
	return undef unless defined $dev;
	my $n = $dirinfo->{"$dev"};
	if (!defined $n) {
		$n = { mnt => $mntpoint, dev => $dev, used => 0 };
		bless $n, "OpenBSD::Vstat::MountPoint";
		$dirinfo->{"$dev"} = $n;
		$dirinfo->{"$mntpoint"} = $n;
		push(@$mnts, $n);
	}
	return $n;
}

sub init_dirinfo()
{
    delete $ENV{'BLOCKSIZE'};
    open(my $cmd2, "/bin/df|") or print STDERR "Can't run df\n";
    while (<$cmd2>) {
	    chomp;
	    if (m/^Filesystem\s+(\d+)\-blocks/) {
		    $blocksize = $1;
	    } elsif (m/^.*?\s+\d+\s+\d+\s+(\d+)\s+\d+\%\s+(\/.*?)$/) {
	    	my ($mntpoint, $avail) = ($2, $1);
		my $i = create_mntpoint($mntpoint);
		next unless defined $i;
		$i->{avail} = $avail;
	    }
    }

    close($cmd2) or print STDERR "Error running df: $!\n";
    open(my $cmd1, "/sbin/mount|") or print STDERR "Can't run mount\n";
    while (<$cmd1>) {
	    chomp;
	    if (m/^.*?\s+on\s+(\/.*?)\s+type\s+.*?(?:\s+\((.*?)\))?$/) {
		my ($mntpoint, $opts) = ($1, $2);
		my $i = create_mntpoint($mntpoint);
		next unless defined $i;
		next unless defined $opts;
		for my $o (split /,\s*/, $opts) {
		    if ($o eq 'read-only') {
			$i->{ro} = 1;
		    } elsif ($o eq 'nodev') {
			$i->{nodev} = 1;
		    } elsif ($o eq 'nosuid') {
			$i->{nosuid} = 1;
		    }
		}
	    } else {
		print STDERR "Can't parse mount line: $_\n";
	    }
    }
    close($cmd1) or print STDERR "Error running mount: $!\n";
}

init_dirinfo();

sub _dirstat($);

sub _dirstat($)
{
	my $dname = shift;
	my $dev = (stat $dname)[0];

	if (!defined $dev) {
		if (!defined $dirinfo->{"$dname"}) {
			return $dirinfo->{"$dname"} = _dirstat(dirname($dname));
		} else {
			return $dirinfo->{"$dname"};
		}
	} else {
		if (!defined $dirinfo->{"$dev"}) {
			return $dirinfo->{"$dev"} = _dirstat(dirname($dname));
		} else {
			return $dirinfo->{"$dev"};
		}
	}
}

sub filestat($)
{
	my $fname = shift;
	my $dev = (stat $fname)[0];

	if (!defined $dev) {
		if (!defined $dirinfo->{"$fname"}) {
			return _dirstat(dirname($fname));
		} else {
			return $dirinfo->{"$fname"};
		}
	} else {
		if (!defined $dirinfo->{"$dev"}) {
			return _dirstat(dirname($fname));
		} else {
			return $dirinfo->{"$dev"};
		}
	}
}

sub vexists($)
{
	my $name = shift;
	if (defined $virtual->{"$name"}) {
		return $virtual->{"$name"};
	} else {
		return -e $name;
	}
}

sub vreaddir($)
{
	my $dirname = shift;
	my %l;
	my $d = gensym;
	opendir($d, $dirname);
	%l = map { $_ => 1 } readdir($d);
	closedir($d);
	if (defined $virtual_dir->{"$dirname"}) {
		for my $e (@{$virtual_dir->{"$dirname"}}) {
			my $n = basename($e);
			if (vexists $e) {
				$l{"$n"} = 1;
			} else {
				undef $l{"$n"};
			}
		}
	}
	return keys(%l);
}

sub account_for($$)
{
	my ($name, $size) = @_;
	my $e = filestat($name);
	$e->{used} += $size;
	return $e;
}

sub add($$)
{
	my ($name, $size) = @_;
	$virtual->{$name} = 1;
	my $d = dirname($name);
	$virtual_dir->{$d} = [] unless defined $virtual_dir->{$d};
	push(@{$virtual_dir->{$d}}, $name);
	return defined($size) ? account_for($name, $size) : undef;
}

sub remove($$)
{
	my ($name, $size) = @_;
	$virtual->{$name} = 0;
	my $d = dirname($name);
	$virtual_dir->{$d} = [] unless defined $virtual_dir->{$d};
	push(@{$virtual_dir->{$d}}, $name);
	return defined($size) ? account_for($name, -$size) : undef;
}

sub tally()
{
	for my $mntpoint (@$mnts) {
		if ($mntpoint->{used} != 0) {
			print $mntpoint->{mnt}, ": ", $mntpoint->{used}, " bytes\n";
		}
	}
}

package OpenBSD::Vstat::MountPoint;
sub avail
{
	my $self = $_[0];

	return $self->{avail} - $self->{used}/$blocksize;
}

1;
