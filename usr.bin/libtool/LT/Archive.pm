# $OpenBSD: Archive.pm,v 1.8 2023/07/06 08:29:26 espie Exp $

# Copyright (c) 2007-2010 Steven Mestdagh <steven@openbsd.org>
# Copyright (c) 2012 Marc Espie <espie@openbsd.org>
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

use v5.36;

package LT::Archive;
use LT::Trace;
use LT::Exec;
use LT::UList;
use LT::Util;
use File::Path;

sub extract($self, $dir, $archive)
{
	if (! -d $dir) {
		tsay {"mkdir -p $dir"};
		File::Path::mkpath($dir);
	}
	LT::Exec->chdir($dir)->link('ar', 'x', $archive);
}

sub get_objlist($self, $a)
{
	open(my $arh, '-|', 'ar', 't', $a);
	my @o = <$arh>;
	close $arh;
	map { chomp; } @o;
	return @o;
}

sub get_symbollist($self, $filepath, $regex, $objlist)
{

	if (@$objlist == 0) {
		die "get_symbollist: object list is empty\n";
	}

	tsay {"generating symbol list in file: $filepath"};
	tsay {"object list is @$objlist" };
	my $symbols = LT::UList->new;
	open(my $sh, '-|', 'nm', '--', @$objlist) or 
	    die "Error running nm on object list @$objlist\n";
	my $c = 0;
	while (my $line = <$sh>) {
		chomp $line;
		tsay {"$c: $line"};
		if ($line =~ m/\S+\s+[BCDEGRST]\s+(.*)/) {
			my $s = $1;
			if ($s =~ m/$regex/) {
				push @$symbols, $s;
				tsay {"matched"};
			}
		}
		$c++;
	}
	open(my $fh, '>', $filepath) or die "Cannot open $filepath\n";
	print $fh map { "$_\n" } sort @$symbols;
}

1;
