# $OpenBSD: Vstat.pm,v 1.1 2003/12/21 18:41:23 espie Exp $
#
# Copyright (c) 2003 Marc Espie.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
# PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

sub init_dirinfo()
{
    open(my $cmd1, "/sbin/mount|") or print STDERR "Can't run mount\n";
    while (<$cmd1>) {
	    chomp;
	    if (m/^.*?\s+on\s+(.*?)\s+type\s+.*?\s+\((.*?)\)$/) {
		my ($mntpoint, $opts) = ($1, $2);
		$dirinfo->{"$mntpoint"} = { mnt => $mntpoint } 
		    unless defined $dirinfo->{"$mntpoint"};
		my $i = $dirinfo->{"$mntpoint"};
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
    close($cmd1);

    delete $ENV{'BLOCKSIZE'};
    open(my $cmd2, "/bin/df|") or print STDERR "Can't run df\n";
    my $bs;
    while (<$cmd2>) {
	    chomp;
	    if (m/^Filesystem\s+(\d+)\-blocks/) {
		    $bs = $1;
	    } elsif (m/^.*?\s+\d+\s+\d+\s+(\d+)\s+\d+\%\s+(.*?)$/) {
	    	my ($mntpoint, $avail) = ($2, $1);
		$dirinfo->{"$mntpoint"} = { mnt => $mntpoint } 
		    unless defined $dirinfo->{"$mntpoint"};
		my $i = $dirinfo->{"$mntpoint"};
		$i->{blocksize} = $bs;
		$i->{avail} = $avail;
	    }
    }
    close($cmd2);
}

init_dirinfo();

sub _dirstat($);

sub _dirstat($)
{
	my $dname = shift;
	
	if (!defined $dirinfo->{"$dname"}) {
		return $dirinfo->{"$dname"} = _dirstat(dirname($dname));
	} else {
		return $dirinfo->{"$dname"};
	}
}

sub filestat($)
{
	my $fname = shift;
	if (!defined $dirinfo->{"$fname"}) {
		return _dirstat(dirname($fname));
	} else {
		return $dirinfo->{"$fname"};
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

sub add($$)
{
	my ($name, $size) = @_;
	$virtual->{$name} = 1;
	my $d = dirname($name);
	$virtual_dir->{$d} = [] unless defined $virtual_dir->{$d};
	push(@{$virtual_dir->{$d}}, $name);
	if (defined $size) {
	    my $e = filestat($name);
	    if (defined $e->{avail} && defined $e->{blocksize}) {
		$e->{avail} -= $size / $e->{blocksize};
		return $e;
	    }
	}
	return undef;
}

sub remove($$)
{
	my ($name, $size) = @_;
	$virtual->{$name} = 0;
	my $d = dirname($name);
	$virtual_dir->{$d} = [] unless defined $virtual_dir->{$d};
	push(@{$virtual_dir->{$d}}, $name);
	if (defined $size) {
	    my $e = filestat($name);
	    $e->{avail} += $size / $e->{blocksize};
	    return $e;
	} else {
		return undef;
	}
}

1;
