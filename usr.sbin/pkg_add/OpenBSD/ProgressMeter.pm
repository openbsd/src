# ex:ts=8 sw=4:
# $OpenBSD: ProgressMeter.pm,v 1.1 2004/10/18 12:03:19 espie Exp $
#
# Copyright (c) 2004 Marc Espie <espie@openbsd.org>
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
#

use strict;
use warnings;

package OpenBSD::ProgressMeter;

my $header;
my $lastdisplay = '';
my $isatty;
my $enabled = 0;

# unless we know better
my $width = 80;
my $playfield;

sub enable
{
	$enabled = 1;
}

sub set_header
{
	$header = shift;
	if (!$enabled) {
		$isatty = 0;
	} else {
		if (!defined $isatty) {
			$isatty = -t STDERR;
		}
	}
	if ($isatty) {
		# compute playfield
		$playfield = $width - length($header) - 10;
		if ($playfield > 40) {
			$playfield = 40;
		}
	}
	return $isatty;
}

sub show
{
	my ($current, $total) = @_;
	return unless $isatty;

        my $stars = ($current * $playfield) / $total;
	my $percent = int (($current * 100)/$total + 0.5);
        my $d = "$header|".'*'x$stars.' 'x($playfield-$stars)."| ".$percent."\%";
	return if $d eq $lastdisplay;
        $lastdisplay=$d;
        print STDERR $d, "\r";
}

sub clear
{
	return unless $isatty;
	print STDERR ' 'x length($lastdisplay), "\r";
}

sub next
{
	return unless $isatty;
	clear;
	print STDERR"$header: complete\n";
}

1;
