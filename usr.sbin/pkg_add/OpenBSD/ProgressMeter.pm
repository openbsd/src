# ex:ts=8 sw=4:
# $OpenBSD: ProgressMeter.pm,v 1.4 2004/12/29 01:11:13 espie Exp $
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
my $width;
my $playfield;

my $wsz_format = 'SSSS';
our %sizeof;

sub find_window_size
{
	return if defined $width;
	# try to get exact window width
	my $r;
	$r = pack($wsz_format, 0, 0, 0, 0);
	$sizeof{'struct winsize'} = 8;
	require 'sys/ttycom.ph';
	$width = 80;
	if (ioctl(STDERR, &TIOCGWINSZ, $r) == 0) {
		my ($rows, $cols, $xpix, $ypix) = 
		    unpack($wsz_format, $r);
		$width = $cols;
	}
}

sub compute_playfield
{
	return unless $isatty;
	# compute playfield
	$playfield = $width - length($header) - 10;
	if ($playfield < 5) {
		$playfield = 0;
	}
}

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
		find_window_size();
		compute_playfield();
		$SIG{'WINCH'} = sub {
			$width = undef;
			find_window_size();
			compute_playfield();
		};
	}
	return $isatty;
}

sub show
{
	my ($current, $total) = @_;
	return unless $isatty;
	my $d;

	if ($playfield) {
	    my $stars = int (($current * $playfield) / $total + 0.5);
	    my $percent = int (($current * 100)/$total + 0.5);
	    $d = "$header|".'*'x$stars.' 'x($playfield-$stars)."| ".$percent."\%";
	} else {
	    $d = $header;
	}
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
