# $OpenBSD: Util.pm,v 1.7 2023/07/08 08:15:32 espie Exp $

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
package LT::Util;
require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(abs_dir $ltdir $version shortdie);
use File::Basename;
use Cwd;

our $ltdir = '.libs';
our $version = '1.5.26'; # pretend to be this version of libtool

sub abs_dir($a)
{
	return dirname(Cwd::abs_path($a));
}

sub shortdie(@p)
{
	$SIG{__DIE__} = 'DEFAULT';
	die @p;
}

1;
