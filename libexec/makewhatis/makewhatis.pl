#!/usr/bin/perl -w
# ex:ts=8 sw=4:

# $OpenBSD: makewhatis.pl,v 1.22 2002/10/15 15:56:16 millert Exp $
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

require 5.006_000;

use strict;
use File::Find;
use File::Temp qw/tempfile/;
use File::Compare;

use Getopt::Std;

my ($picky, $testmode);

# write_uniques($list, $file):
#
#   write $list to file named $file, removing duplicate entries.
#   Change $file mode/owners to expected values
#   Write to temporary file first, and do the copy only if changes happened.
#
sub write_uniques
{
    my $list = shift;
    my $f = shift;
    local $_;

    my ($out, $tempname);
    ($out, $tempname) = tempfile('/tmp/makewhatis.XXXXXXXXXX') or die "$0: Can't open temporary file";

    my @sorted = sort @$list;
    my $last;

    while ($_ = shift @sorted) {
	print $out $_, "\n" unless defined $last and $_ eq $last;
	$last = $_;
    }
    close $out;
    if (compare($tempname, $f) == 0) {
    	unlink($tempname);
    } else {
    	use File::Copy;

	unlink($f);
	if (move($tempname, $f)) {
	    chmod 0444, $f;
	    chown 0, (getgrnam 'bin')[2], $f;
	} else {
	    print STDERR "$0: Can't create $f ($!)\n";
	    unlink($tempname);
	    exit 1;
	}
    }
}

sub found
{
    my @candidates = glob shift;
    return @candidates > 1 || @candidates == 1 && -e $candidates[0];
}

# verify_subject($subject, $filename):
#
#   reparse the subject we're about to add, and check whether it makes
#   sense, e.g., is there a man page around.
sub verify_subject
{
    local $_ = shift;
    my $filename = shift;
    if (m/\s*(.*?)\s*\((.*?)\)\s-\s/) {
    	my $man = $1;
	my $section = $2;
	my @mans = split(/\s*,\s*|\s+/, $man);
	my $base = $filename;
	if ($base =~ m|/|) {
	    $base =~ s,/[^/]*$,,;
	} else {
		$base = '.';
	}
	for my $i (@mans) {
	    next if found("$base/$i.*");
	    # try harder
	    $i =~ s/\(\)//;
	    $i =~ s/\-//g;
	    $i =~ s,^etc/,,;
	    next if found("$base/$i.*");
	    # and harder...
	    $i =~ tr/[A-Z]/[a-z]/;
	    next if found("$base/$i.*");
	    print STDERR "Couldn't find $i in $filename:\n$_\n" 
	}
    }
}


# add_unformated_subject($lines, $toadd, $section, $filename, $toexpand):
#
#   build subject from list of $toadd lines, and add it to the list
#   of current subjects as section $section
#
sub add_unformated_subject
{
    my $subjects = shift;
    my $toadd = shift;
    my $section = shift;
    my $filename = shift;
    my $toexpand = shift;

    my $exp = sub {
    	if (defined $toexpand->{$_[0]}) {
		return $toexpand->{$_[0]};
	} else {
		print STDERR "$filename: can't expand $_[0]\n";
		return "";
	}
    };

    local $_ = join(' ', @$toadd);
	# do interpolations
    s/\\\*\((..)/&$exp($1)/ge;
    s/\\\*\[(.*?)\]/&$exp($1)/ge;

	# horizontal space adjustments
    while (s/\\s[-+]?\d+//g)
    	{}
	# unbreakable spaces
    s/\\\s+/ /g;
    	# unbreakable em dashes
    s/\\\|\\\(em\\\|/-/g;
	# em dashes
    s/\\\(em\s+/- /g;
    	# em dashes in the middle of lines
    s/\\\(em/-/g;
    s/\\\*[LO]//g;
    s/\\\(tm/(tm)/g;
	# font changes
    s/\\f[BIRP]//g;
    s/\\f\(..//g;
    	# fine space adjustments
    while (s/\\[vh]\'.*?\'//g)
    	{}
    unless (s/\s+\\-\s+/ ($section) - / || s/\\\-/($section) -/ ||
    	s/\s-\s/ ($section) - /) {
	print STDERR "Weird subject line in $filename:\n$_\n" if $picky;
	    # Try guessing where the separation falls...
	s/\S+\s+/$& ($section) - / || s/\s*$/ ($section) - (empty subject)/;
    }
	# other dashes
    s/\\-/-/g;
	# escaped characters
    s/\\\&(.)/$1/g;
    s/\\\|/|/g;
	# gremlins...
    s/\\c//g;
	# sequence of spaces
    s/\s+$//;
    s/^\s+//;
    s/\s+/ /g;
    	# some damage control
    if (m/^\Q($section) - \E/) {
    	print STDERR "Rejecting non-subject line from $filename:\n$_\n"
	    if $picky;
	return;
    }
    push(@$subjects, $_);
    verify_subject($_, $filename) if $picky;
}

# $lines = handle_unformated($file)
#
#   handle an unformated manpage in $file
#
#   may return several subjects, perl(3p) do !
#
sub handle_unformated
{
    my $f = shift;
    my $filename = shift;
    my @lines = ();
    my %toexpand = ();
    my $so_found = 0;
    local $_;
	# retrieve basename of file
    my ($name, $section) = $filename =~ m|(?:.*/)?(.*)\.([\w\d]+)|;
	# scan until macro
    while (<$f>) {
	next unless m/^\./;
	if (m/^\.\s*de/) {
	    while (<$f>) {
		last if m/^\.\s*\./;
	    }
	    next;
	}
	if (m/^\.\s*ds\s+(\S+)\s+/) {
	    chomp($toexpand{$1} = $');
	    next;
	}
	    # Some cross-refs just link to another manpage
	$so_found = 1 if m/\.so/;
	if (m/^\.\s*TH/ || m/^\.\s*th/) {
		# in pricky mode, we should try to match these
	    # ($name2, $section2) = m/^\.(?:TH|th)\s+(\S+)\s+(\S+)/;
	    	# scan until first section
	    while (<$f>) {
		if (m/^\.\s*de/) {
		    while (<$f>) {
			last if m/^\.\s*\./;
		    }
		    next;
		}
		if (m/^\.\s*ds\s+(\S+)\s+/) {
		    chomp($toexpand{$1} = $');
		    next;
		}
		next unless m/^\./;
		if (m/^\.\s*SH/ || m/^\.\s*sh/) {
		    my @subject = ();
		    while (<$f>) {
			last if m/^\.\s*(?:SH|sh|SS|ss|nf|LI)/;
			    # several subjects in one manpage
			if (m/^\.\s*(?:PP|Pp|br|PD|LP|sp)/) {
			    add_unformated_subject(\@lines, \@subject,
				$section, $filename, \%toexpand)
				    if @subject != 0;
			    @subject = ();
			    next;
			}
			next if m/^\'/ || m/^\.\s*tr\s+/ || m/^\.\s*\\\"/ ||
			    m/^\.\s*sv/ || m/^\.\s*Vb\s+/ || m/\.\s*HP\s+/;
			if (m/^\.\s*de/) {
			    while (<$f>) {
				last if m/^\.\s*\./;
			    }
			    next;
			}
			if (m/^\.\s*ds\s+(\S+)\s+/) {
			    chomp($toexpand{$1} = $');
			    next;
			}
			# Motif index entries, don't do anything for now.
			next if m/^\.\s*iX/;
			# Some other index (cook)
			next if m/^\.\s*XX/;
			chomp;
			s/\.\s*(?:B|I|IR|SM|BR)\s+//;
			if (m/^\.\s*(\S\S)/) {
			    print STDERR "$filename: not grokking $_\n" 
				if $picky;
			    next;
			}
			push(@subject, $_) unless m/^\s*$/;
		    }
		    add_unformated_subject(\@lines, \@subject, $section,
			$filename, \%toexpand) if @subject != 0;
		    return \@lines;
		}
	    }
	    print STDERR "Couldn't find subject in old manpage $filename\n";
	} elsif (m/^\.\s*Dt/) {
	    $section .= "/$1" if (m/^\.\s*Dt\s+\S+\s+\d\S*\s+(\S+)/);
	    while (<$f>) {
		next unless m/^\./;
		if (m/^\.\s*Sh/) {
		    # subject/keep is the only way to deal with Nm/Nd pairs
		    my @subject = ();
		    my @keep = ();
		    my $nd_seen = 0;
		    while (<$f>) {
		    	next if m/^\.\\\"/;
			last if m/^\.\s*Sh/;
			s/\s,/,/g;
			if (s/^\.\s*(\S\S)\s+//) {
			    my $macro = $1;
			    next if $macro eq "\\\"";
			    s/\"(.*?)\"/$1/g;
			    s/\\-/-/g;
			    $macro eq 'Xr' and s/^(\S+)\s+(\d\S*)/$1 ($2)/;
			    $macro eq 'Ox' and s/^/OpenBSD /;
			    $macro eq 'Nx' and s/^/NetBSD /;
			    if ($macro eq 'Nd') {
				if (@keep != 0) {
				    add_unformated_subject(\@lines, \@keep, 
				    	$section, $filename, \%toexpand);
				    @keep = ();
				}
				push(@subject, "\\-");
				$nd_seen = 1;
			    }
			    if ($nd_seen && $macro eq 'Nm') {
				@keep = @subject;
				@subject = ();
				$nd_seen = 0;
			    }
			}
			push(@subject, $_) unless m/^\s*$/;
		    }
		    unshift(@subject, @keep) if @keep != 0;
		    add_unformated_subject(\@lines, \@subject, $section,
		    	$filename, \%toexpand)
			    if @subject != 0;
		    return \@lines;
		}
	    }
	}
    }
    if ($so_found == 0) {
	print STDERR "Unknown manpage type $filename\n";
    }
    return \@lines;
}
			
# add_formated_subject($subjects, $_, $section):
#   add subject $_ to the list of current $subjects, in section $section.
#
sub add_formated_subject
{
    my $subjects = shift;
    local $_ = shift;
    my $section = shift;
    my $filename = shift;

    # some twits underline the command name
    while (s/_\cH//g || s/(.)\cH\1/$1/g)
	{}
    if (m/-/) {
	s/([-+.\w\d,])\s+/$1 /g;
	s/([a-z][A-z])-\s+/$1/g;
	# some twits use: func -- description
	if (m/^[^-+.\w\d]*(.*) -(?:-?)\s+(.*)/) {
	    my ($func, $descr) = ($1, $2);
	    $func =~ s/,\s*$//;
	    # nroff will tend to cut function names at the weirdest places
	    if (length($func) > 40 && $func =~ m/,/ && $section =~ /^3/) {
	    	$func =~ s/\b \b//g;
	    }
	    $_ = "$func ($section) - $descr";
	    verify_subject($_, $filename) if $picky;
	    push(@$subjects, $_);
	    return;
	}
    }

    print STDERR "Weird subject line in $filename:\n$_\n" if $picky;

    # try to find subject in line anyway
    if (m/^\s*(.*\S)(?:\s{3,}|\(\)\s+)(.*?)\s*$/) {
    	my ($func, $descr) = ($1, $2);
	$func =~ s/\s+/ /g;
	$descr =~ s/\s+/ /g;
	$_ = "$func ($section) - $descr";
	verify_subject($_, $filename) if $picky;
	push(@$subjects, $_);
	return;
    }

    print STDERR "Weird subject line in $filename:\n$_\n" unless $picky;
}

# $lines = handle_formated($file)
#
#   handle a formatted manpage in $file
#
#   may return several subjects, perl(3p) do !
#
sub handle_formated
{
    my $file = shift;
    my $filename = shift;
    local $_;
    my ($section, $subject);
    my @lines=();
    while (<$file>) {
	next if /^$/;
	chomp;
	# Remove boldface from wide characters
	while (s/(..)\cH\cH\1/$1/g)
	    {}
	# Remove boldface and underlining
	while (s/_\cH//g || s/(.)\cH\1/$1/g)
	    {}
	if (m/\w[-+.\w\d]*\(([-+.\w\d\/]+)\)/) {
	    $section = $1;
	    # Find architecture
	    if (m/Manual\s+\((.*?)\)/) {
		$section = "$section/$1";
	    }
	}
	# Not all man pages are in english
	# weird hex is `Namae' in japanese
	if (m/^(?:NAME|NAMES|NAMN|Name|\xbe|\xcc\xbe\xbe\xce|\xcc\xbe\xc1\xb0)\s*$/) {
	    unless (defined $section) {
		# try to retrieve section from filename
		if ($filename =~ m/(?:cat|man)([\dln])\//) {
		    $section = $1;
		    print STDERR "Can't find section in $filename, deducting $section from context\n" if $picky;
		} else {
		    $section='??';
		    print STDERR "Can't find section in $filename\n";
		}
	    }
	    while (<$file>) {
		chomp;
		# perl agregates several subjects in one manpage
		if (m/^$/) {
		    add_formated_subject(\@lines, $subject, $section, $filename) 
			if defined $subject;
		    $subject = undef;
		} elsif (m/^\S/ || m/^\s+\*{3,}\s*$/) {
		    add_formated_subject(\@lines, $subject, $section, $filename) 
			if defined $subject;
		    last;
		} else {
		    # deal with troff hyphenations
		    if (defined $subject and $subject =~ m/\xad\s*$/) {
		    	$subject =~ s/(?:\xad\cH)*\xad\s*$//;
			s/^\s*//;
		    }
		    $subject.=$_;
		}
	    }
	last;
	}
    }

    print STDERR "Can't parse $filename (not a manpage ?)\n" if @lines == 0;
    return \@lines;
}

# $list = find_manpages($dir)
#
#   find all manpages under $dir, trim some duplicates.
#
sub find_manpages
{
    my $dir = shift;
    my ($list, %nodes);
    $list=[];
    find(
	sub {
	return unless /\.[\dln]\w*(?:\.Z|\.gz)?$/;
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
#   scan a set of manpages, return list of subjects
#
sub scan_manpages
{
    my $list = shift;
    local $_;
    my ($done);
    $done=[];

    for (@$list) {
	my ($file, $subjects);
	if (m/\.(?:Z|gz)$/) {
	    unless (open $file, '-|', "gzip -fdc $_") {
	    	warn "$0: Can't decompress $_\n";
		next;
	    }
	    $_ = $`;
	} else {
	    unless (open $file, '<', $_) {
	    	warn "$0: Can't read $_\n";
		next;
	    }
	}
	if (m/\.[1-9ln][^.]*$/) {
	    $subjects = handle_unformated($file, $_);
	} elsif (m/\.0$/) {
	    $subjects = handle_formated($file, $_);
	    # in test mode, we try harder
	} elsif ($testmode) {
	    $subjects = handle_unformated($file, $_);
	    if (@$subjects == 0) {
	    	$subjects = handle_formated($file, $_);
	    }
	} else {
	    print STDERR "Can't find type of $_";
	    next;
	}
	push @$done, @$subjects;
    }
    return $done;
}

# build_index($dir)
#
#   build index for $dir
#
sub build_index
{
    my $dir = shift;
    my $list = find_manpages($dir);
    my $subjects = scan_manpages($list);
    write_uniques($subjects, "$dir/whatis.db");
}

# main code
    
my %opts;
getopts('tpd:', \%opts);

if (defined $opts{'p'}) {
    $picky = 1;
}
if (defined $opts{'t'}) {
    $testmode = 1;
    my $subjects = scan_manpages(\@ARGV);
    print join("\n", @$subjects), "\n";
    exit 0;
} 

if (defined $opts{'d'}) {
    my $mandir = $opts{'d'};
    unless (-d $mandir) {
	die "$0: $mandir: not a directory"
    }
    chdir $mandir;

    my $whatis = "$mandir/whatis.db";
    open(my $old, '<', $whatis) or
	die "$0 $whatis to merge with";
    my $subjects = scan_manpages(\@ARGV);
    while (<$old>) {
	chomp;
	push(@$subjects, $_);
    }
    close($old);
    write_uniques($subjects, $whatis);
    exit 0;
}
if ($#ARGV == -1) {
    local $_;
    @ARGV=();
    open(my $conf, '<', '/etc/man.conf') or 
	die "$0: Can't open /etc/man.conf";
    while (<$conf>) {
	chomp;
	push(@ARGV, $1) if /^_whatdb\s+(.*)\/whatis\.db\s*$/;
    }
    close $conf;
}
	
for my $mandir (@ARGV) {
    if (-d $mandir) {
	build_index($mandir);
    } else {
    	print STDERR "$0: $mandir is not a directory\n";
    }
}
