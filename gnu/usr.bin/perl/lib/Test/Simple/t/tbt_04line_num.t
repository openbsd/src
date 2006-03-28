#!/usr/bin/perl

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

use Test::More tests => 3;
use Test::Builder::Tester;

is(line_num(),13,"normal line num");
is(line_num(-1),13,"line number minus one");
is(line_num(+2),17,"line number plus two");
