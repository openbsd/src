# $OpenBSD: Archive.pm,v 1.1 2012/06/19 09:30:44 espie Exp $

# Copyright (c) 2007-2010 Steven Mestdagh <steven@openbsd.org>
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
use feature qw(say switch state);

package LT::Archive;
use LT::Trace;
use LT::Exec;
use LT::Util;

sub extract
{
	my ($self, $dir, $archive) = @_;

	if (! -d $dir) {
		LT::Trace::debug {"mkdir -p $dir\n"};
		File::Path::mkpath($dir);
	}
	LT::Exec->chdir($dir)->link('ar', 'x', $archive);
}

sub get_objlist
{
	my ($self, $a) = @_;

	open(my $arh, '-|', 'ar', 't', $a);
	my @o = <$arh>;
	close $arh;
	map { chomp; } @o;
	return @o;
}

sub get_symbollist
{
	my ($self, $filepath, $regex, $objlist) = @_;

	if (@$objlist == 0) {
		die "get_symbollist: object list is empty\n";
	}

	LT::Trace::debug {"generating symbol list in file: $filepath\n"};
	my $symbols = [];
	open(my $sh, '-|', 'nm', @$objlist) or die "Error running nm on object list\n";
	my $c = 0;
	while (my $line = <$sh>) {
		chomp $line;
		LT::Trace::debug {"$c: $line\n"};
		if ($line =~ m/\S+\s+[BCDEGRST]\s+(.*)/) {
			my $s = $1;
			if ($s =~ m/$regex/) {
				push @$symbols, $s;
				LT::Trace::debug {"matched\n"};
			}
		}
		$c++;
	}
	$symbols = reverse_zap_duplicates_ref($symbols);
	@$symbols = sort @$symbols;
	open(my $fh, '>', $filepath) or die "Cannot open $filepath\n";
	print $fh join("\n", @$symbols), "\n";
}

1;
