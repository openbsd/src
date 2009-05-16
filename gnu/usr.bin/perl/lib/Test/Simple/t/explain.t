#!/usr/bin/perl -w
# $Id: explain.t,v 1.1 2009/05/16 21:42:57 simon Exp $

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use warnings;

use Test::More tests => 5;

can_ok "main", "explain";

is_deeply [explain("foo")],             ["foo"];
is_deeply [explain("foo", "bar")],      ["foo", "bar"];

# Avoid future dump formatting changes from breaking tests by just eval'ing
# the dump
is_deeply [map { eval $_ } explain([], {})],           [[], {}];

is_deeply [map { eval $_ } explain(23, [42,91], 99)],  [23, [42, 91], 99];
