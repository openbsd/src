# ex:ts=8 sw=4:
# $OpenBSD: PackageName.pm,v 1.6 2004/10/11 09:44:06 espie Exp $
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

use strict;
use warnings;
package OpenBSD::PackageName;

sub url2pkgname($)
{
	my $name = $_[0];
	$name =~ s|.*/||;
	$name =~ s|\.tgz$||;

	return $name;
}

# see package-specs(7)
sub splitname
{
	local $_ = shift;
	if (/\-(?=\d)/) {
		my $stem = $`;
		my $rest = $';
		my @all = split /\-/, $rest;
		return ($stem, @all);
	} else {
		return ($_);
	}
}

sub splitstem
{
	return (splitname $_[0])[0];
}

sub is_stem
{
	local $_ = shift;
	if (m/\-\d/) {
		return 0;
	} else {
		return 1;
	}
}

sub findstem
{
	my ($k, @list) = @_;
	my @r = ();
	for my $n (@list) {
		my $stem = splitstem($n);
		if ($k eq $stem) {
			push(@r, $n);
		}
	}
	return @r;
}

1;
