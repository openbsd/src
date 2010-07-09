# ex:ts=8 sw=4:
# $OpenBSD: Check.pm,v 1.3 2010/07/09 08:12:49 espie Exp $
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
package OpenBSD::Makewhatis::Check;

sub found
{
    my ($pattern, $filename) = @_;
    my @candidates = glob $pattern;
    if (@candidates > 0) {
    	# quick check of inode, dev number
    	my ($dev_cmp, $inode_cmp) = (stat $filename)[0,1];
    	for my $f (@candidates) {
	    my ($dev, $inode) = (stat $f)[0, 1];
	    if ($dev == $dev_cmp && $inode == $inode_cmp) {
		return 1;
	    }
	}
	# slow check with File::Compare
	require File::Compare;

	for my $f (@candidates) {
	    if (File::Compare::compare($f, $filename) == 0) {
		return 1;
	    }
	}
    }
    return 0;
}
# verify_subject($subject, $filename, $p):
#
#   reparse the subject we're about to add, and check whether it makes
#   sense, e.g., is there a man page around.
sub verify_subject
{
    my ($_, $filename, $p) = @_;
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
	my @notfound = ();
	for my $func (@mans) {
	    my $i = $func;
	    next if found("$base/$i.*", $filename);
	    # try harder
	    $i =~ s/\(\)//;
	    $i =~ s/\-//g;
	    $i =~ s,^etc/,,;
	    next if found("$base/$i.*", $filename);
	    # and harder...
	    $i =~ tr/[A-Z]/[a-z]/;
	    next if found("$base/$i.*", $filename);
	    push(@notfound, $func);
	}
	if (@notfound > 0) {
	    $p->errsay("Couldn't find #1 in #2:\n#3",
	    	join(', ', @notfound), $filename, $_);
	}
    }
}

1;
