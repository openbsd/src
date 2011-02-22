# ex:ts=8 sw=4:
# $OpenBSD: Unformated.pm,v 1.7 2011/02/22 00:23:14 espie Exp $
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
package OpenBSD::Makewhatis::Unformated;

# add_unformated_subject($lines, $toadd, $section, $p) :
#
#   build subject from list of $toadd lines, and add it to the list
#   of current subjects as section $section
#
sub add_unformated_subject
{
    my ($subjects, $toadd, $section, $toexpand, $h) = @_;

    my $exp = sub {
    	if (defined $toexpand->{$_[0]}) {
		return $toexpand->{$_[0]};
	} else {
		$h->errsay("#2: can't expand #1", $_[0]);
		return "";
	}
    };

    my $_ = join(' ', @$toadd);
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
    	# single quotes
    s/\\\(aq/\'/g;
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
    unless (s/\s+\\-\s+/ ($section) - / || s/\s*\\\-/ ($section) -/ ||
    	s/\s-\s/ ($section) - /) {
	$h->weird_subject($_) if $h->p->picky;
	    # Try guessing where the separation falls...
	s/\s+\:\s+/ ($section) - / || s/\S+\s+/$& ($section) - / || s/\s*$/ ($section) - (empty subject)/;
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
    	$h->weird_subject($_) if $h->p->picky;
	return;
    }
    push(@$subjects, $_);
}

# $lines = handle($file, $h)
#
#   handle an unformated manpage in $file
#
#   may return several subjects, perl(3p) do !
#
sub handle
{
    my ($f, $h) = @_;
    my @lines = ();
    my %toexpand = (Tm => '(tm)');
    my $so_found = 0;
    my $found_th = 0;
    my $found_old = 0;
    my $found_dt = 0;
    my $found_new = 0;
    # subject/keep is the only way to deal with Nm/Nd pairs
    my @subject = ();
    my @keep = ();
    my $nd_seen = 0;
    my $_;
	# retrieve basename of file
    my ($name, $section) = $h->filename =~ m|(?:.*/)?(.*)\.([\w\d]+)|;
	# scan until macro
    while (<$f>) {
	next unless m/^\./ || $found_old || $found_new;
	next if m/^\.\\\"/;
	next if m/^\.if\s+t\s+/;
	s/^\.i[ef]\s+n\s+//;
	s/^\.i[ef]\s+\\n\(\.g\s+//;
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
	$so_found = 1 if m/^\.\s*so/;
	if (m/^\.\s*TH/ || m/^\.\s*th/) {
		# in pricky mode, we should try to match these
	    # ($name2, $section2) = m/^\.(?:TH|th)\s+(\S+)\s+(\S+)/;
	    	# scan until first section
	    $found_th = 1;
	    next;
	}
	if ($found_th && !$found_old && (m/^\.\s*SH/ || m/^\.\s*sh/)) {
		$found_old = 1;
		next;
	}
	if (m/^\.\s*Dt/) {
	    $section .= "/$1" if (m/^\.\s*Dt\s+\S+\s+\d\S*\s+(\S+)/);
	    $found_dt = 1;
	    next;
    	}
	if ($found_dt && !$found_new && m/^\.\s*Sh/) {
		$found_new = 1;
		next;
	}
	if ($found_old) {
		last if m/^\.\s*(?:SH|sh|SS|ss|nf|LI)/;
		    # several subjects in one manpage
		if (m/^\.\s*(?:PP|Pp|br|PD|LP|sp)/) {
		    add_unformated_subject(\@lines, \@subject,
			$section, \%toexpand, $h) if @subject != 0;
		    @subject = ();
		    next;
		}
		next if m/^\'/ || m/^\.\s*tr\s+/ || m/^\.\s*\\\"/ ||
		    m/^\.\s*sv/ || m/^\.\s*Vb\s+/ || m/\.\s*HP\s+/;
		# Motif index entries, don't do anything for now.
		next if m/^\.\s*iX/;
		# Some other index (cook)
		next if m/^\.\s*XX/;
		chomp;
		s/\.\s*(?:B|I|IR|SM|BR)\s+//;
		if (m/^\.\s*(\S\S)/) {
		    $h->errsay("#2: not grokking #1", $_)
			if $h->p->picky;
		    next;
		}
		push(@subject, $_) unless m/^\s*$/;
		next;
	}
	if ($found_new) {
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
				$section, \%toexpand, $h);
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
    }
    if ($found_th && !$found_old) {
    		$h->cant_find_subject;
    }
    if ($found_dt && !$found_new) {
    		$h->cant_find_subject;
    }
    unshift(@subject, @keep) if @keep != 0;
    add_unformated_subject(\@lines, \@subject, $section,
	\%toexpand, $h) if @subject != 0;
    if (!$so_found && !$found_old && !$found_new) {
    	$h->errsay("Unknown manpage type #1");
    }
    return \@lines;
}

1;
