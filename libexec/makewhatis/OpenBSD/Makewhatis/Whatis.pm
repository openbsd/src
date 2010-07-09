# ex:ts=8 sw=4:
# $OpenBSD: Whatis.pm,v 1.4 2010/07/09 08:12:49 espie Exp $
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
package OpenBSD::Makewhatis::Whatis;

use constant MAXLINELEN => 8192;

use File::Temp qw/tempfile/;
use File::Compare;

# write($list, $dir):
#
#   write $list to file named $file, removing duplicate entries.
#   Change $file mode/owners to expected values
#   Write to temporary file first, and do the copy only if changes happened.
#
sub write
{
    my ($list, $dir, $p) = @_;
    my $f = "$dir/whatis.db";

    my ($out, $tempname);
    ($out, $tempname) = tempfile('/tmp/makewhatis.XXXXXXXXXX') or die "$0: Can't open temporary file";

    my @sorted = sort @$list;
    my $last;

    while (my $l = shift @sorted) {
    	next if length $l > MAXLINELEN;
	print $out $l, "\n" unless defined $last and $l eq $last;
	$last = $l;
    }
    close $out;
    if (compare($tempname, $f) == 0) {
    	unlink($tempname);
    } else {
	require File::Copy;

	unlink($f);
	if (File::Copy::move($tempname, $f)) {
	    chmod 0444, $f;
	    chown 0, (getgrnam 'bin')[2], $f;
	} else {
	    $p->errsay("#1: Can't create #2: #3", $0, $f, $!);
	    unlink($tempname);
	    exit 1;
	}
    }
}

1;
