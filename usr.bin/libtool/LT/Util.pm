# $OpenBSD: Util.pm,v 1.1 2012/06/19 09:30:44 espie Exp $

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
package LT::Util;
require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(reverse_zap_duplicates_ref abs_dir $ltdir $version
    @picflags $sharedflag);
use File::Basename;
use Cwd;

our $ltdir = '.libs';
our $version = '1.5.26'; # pretend to be this version of libtool
our @picflags = ('-fPIC', '-DPIC');
our $sharedflag = '-shared';

# walk a list from back to front, removing any duplicates
# this should make sure a library's dependencies are behind the library itself
sub reverse_zap_duplicates_ref
{
	my $arglist = shift;
	my $h = {};
	my $r = [];
	for my $el (reverse @$arglist) {
		next if defined $h->{$el};
		unshift @$r, $el;
		$h->{$el} = 1;
	}
	return $r;
}

sub abs_dir
{
	my $a = shift;
	return dirname(Cwd::abs_path($a));
}

1;
