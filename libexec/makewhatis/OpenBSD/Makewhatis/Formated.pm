# ex:ts=8 sw=4:
# $OpenBSD: Formated.pm,v 1.2 2004/12/24 00:00:04 espie Exp $
# Copyright (c) 2000-2004 Marc Espie <espie@openbsd.org>
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

use strict;
use warnings;
package OpenBSD::Makewhatis::Formated;

# add_formated_subject($subjects, $_, $section):
#   add subject $_ to the list of current $subjects, in section $section.
#
sub add_formated_subject
{
    my ($subjects, $line, $section, $filename, $picky) = @_;
    local $_ = $line;

    if (m/-/) {
	s/([-+.\w\d,])\s+/$1 /g;
	s/([a-z][A-z])-\s+/$1/g;
	# some twits use: func -- description
	if (m/^[^-+.\w\d]*(.*?) -(?:-?)\s+(.*)/) {
	    my ($func, $descr) = ($1, $2);
	    $func =~ s/,\s*$//;
	    # nroff will tend to cut function names at the weirdest places
	    if (length($func) > 40 && $func =~ m/,/ && $section =~ /^3/) {
	    	$func =~ s/\b \b//g;
	    }
	    $_ = "$func ($section) - $descr";
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
	push(@$subjects, $_);
	return;
    }

    print STDERR "Weird subject line in $filename:\n$_\n" unless $picky;
}

# $lines = handle($file, $filename, $picky)
#
#   handle a formatted manpage in $file
#
#   may return several subjects, perl(3p) do !
#
sub handle
{
    my ($file, $filename, $picky) = @_;
    local $_;
    my ($section, $subject);
    my @lines=();
    my $foundname = 0;
    while (<$file>) {
	chomp;
	if (/^$/) {
	    # perl aggregates several subjects in one manpage
	    # so we don't stop after we've got one subject
	    add_formated_subject(\@lines, $subject, $section, $filename, $picky) 
		if defined $subject;
	    $subject = undef;
	    next;
	}
	# Remove boldface from wide characters
	while (s/(..)\cH\cH\1/$1/g)
	    {}
	# Remove boldface and underlining
	while (s/_\cH//g || s/(.)\cH\1/$1/g)
	    {}
	if (!$foundname && m/\w[-+.\w\d]*\(([-+.\w\d\/]+)\)/) {
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
	    $foundname = 1;
	    next;
	}
	if ($foundname) {
	    if (m/^\S/ || m/^\s+\*{3,}\s*$/) {
		add_formated_subject(\@lines, $subject, $section, $filename, $picky) 
		    if defined $subject;
		last;
	    } else {
		# deal with troff hyphenations
		if (defined $subject and $subject =~ m/\xad\s*$/) {
		    $subject =~ s/(?:\xad\cH)*\xad\s*$//;
		    s/^\s*//;
		}
		# more troff hyphenation
		if (defined $subject and $subject =~ m/\S(?:\-\cH)*\-$/) {
		    $subject =~ s/(?:\-\cH)*\-$//;
		    s/^\s*//;
		}
		s/^\s+/ /;
		$subject.=$_;
	    }
	}
    }

    print STDERR "Can't parse $filename (not a manpage ?)\n" if @lines == 0;
    return \@lines;
}

1;
