#!/usr/bin/perl -w

# $OpenBSD: makewhatis.pl,v 1.1 2000/02/03 18:10:48 espie Exp $
#
# Copyright (c) 2000 Marc Espie.
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

use strict;
use File::Find;
use IO::File;


# write_uniques($list, $file):
#
#	write $list to file named $file, removing duplicate entries.
#	Change $file mode/owners to expected values
#
sub write_uniques
{
	my $list = shift;
	my $f = shift;
	my ($out, $last);
	local $_;

	$out = new IO::File $f, "w" or die "$0: Can't open $f";

	my @sorted = sort @$list;

	while ($_ = shift @sorted) {
		print $out $_, "\n" unless defined $last and $_ eq $last;
		$last = $_;
	}
	close $out;
	chmod 0444, $f;
	chown 0, (getgrnam 'bin')[2], $f;
}

# handle_unformated($result, $args)
#
#	handle a batch of unformated manpages $args,
#	push the subjects to $result
#
sub handle_unformated
{
	my $result = shift;
	my $args = shift;
	local $_;
	my $cmd;

	$cmd = new IO::File "/usr/libexec/getNAME ".join(" ", @$args)."|";
	while (<$cmd>) {
		chomp;
		s/ [a-zA-Z\d]* \\-/ -/;
		push(@$result, $_);
	}
	close $cmd;
}

# $line = handle_formated($file)
#
#	handle a formatted manpage in $file
#
sub handle_formated
{
	my $file = shift;
	local $_;
	my ($section, $subject);
	while (<$file>) {
		chomp;
		last if m/^N\cHNA\cHAM\cHME\cHE/;
		if (m/\w[-+.\w\d]*\(([\/\-+.\w\d]+)\)/) {
			$section = $1;
		}
	}
	while (<$file>) {
	    chomp;
	    last if m/^\S/;
	    # some twits underline the command name
	    s/_\cH//g;
	    $subject.=$_;
	}
	$_ = $subject;
	if (defined $_ and m/-/) {
		s/.\cH//g;
		s/([-+.,\w\d])\s+/$1 /g;
		s/([a-z][A-z])-\s+/$1/g;
		s/^[^-+.\w\d]*(.*) - (.*)$/$1 ($section) - $2/;
		return $_;
	} else {
		return undef;
	}
}

# $list = find_manpages($dir)
#
#	find all manpages under $dir, trim some duplicates.
#
sub find_manpages
{
	my $dir = shift;
	my ($list, %nodes);
	$list=[];
	find(
	    sub {
		return unless /(?:\.[0-9]|0\.Z|0\.gz)$/;
		return unless -f $_;
		my $inode = (stat _)[1];
		return if defined $nodes{$inode};
		$nodes{$inode} = 1;
		push(@$list, $File::Find::name);
	    }, $dir);
	return $list;
}

# $subjects = scan_manpages($list)
#
#	scan a set of manpages, return list of subjects
#
sub scan_manpages
{
	my $list = shift;
	local $_;
	my (@todo, $done);
	$done=[];

	for (@$list) {
	    my ($file, $line);
	    if (m/\.[1-9]$/) {
		    push(@todo, $_);
		    if (@todo > 5000) {
			    handle_unformated($done, \@todo);
			    @todo = ();
		    }
		    next;
	    } elsif (m/\.0\.(?:Z|gz)$/) {
		    $file = new IO::File "gzip -fdc $_|";
	    } else {
		    $file = new IO::File $_ or die "$0: Can't read $_\n";
	    }

	    $line = handle_formated($file);
	    push @$done, $line if defined $line;
	}
	if (@todo > 0) {
		handle_unformated($done, \@todo);
	}
	return $done;
}

# build_index($dir)
#
#	build index for $dir
sub build_index
{
	my $dir = shift;
	my $list = find_manpages($dir);
	my $subjects = scan_manpages($list);
	write_uniques($subjects, "$dir/whatis.db");
}


# main code
    
while ($#ARGV != -1 and $ARGV[0] =~ m/^-/) {
	my $opt = shift;
	last if $opt eq '--';
	if ($opt eq '-d') {
		my $mandir = shift;
		unless (-d $mandir) {
			die "$0: $mandir: not a directory"
		}
		chdir $mandir;

		my $whatis = "$mandir/whatis.db";
		my $old = new IO::File $whatis or 
		    die "$0 $whatis to merge with";
		my $subjects = scan_manpages(\@ARGV);
		while (<$old>) {
			chomp;
			push(@$subjects, $_);
		}
		close $old;
		write_uniques($subjects, $whatis);
		exit 0;
	} else {
		die "$0: unknown option $opt\n";
	}
}

if ($#ARGV == -1) {
	local $_;
	@ARGV=();
	my $conf;
	$conf = new IO::File '/etc/man.conf' or 
	    die "$0: Can't open /etc/man.conf";
	while (<$conf>) {
		chomp;
		push(@ARGV, $1) if /^_whatdb\s+(.*)\/whatis\.db\s*$/;
	}
	close $conf;
}
		
for my $mandir (@ARGV) {
	unless (-d $mandir) {
		die "$0: $mandir: not a directory"
	}
    build_index($mandir);
}


