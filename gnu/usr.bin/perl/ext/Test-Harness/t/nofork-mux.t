#!/usr/bin/perl -w

BEGIN {
    if ( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ( '../lib', '../ext/Test-Harness/t/lib' );
    }
    else {
        use lib 't/lib';
    }
}

use strict;

use NoFork;
require(
    ( $ENV{PERL_CORE} ? '../ext/Test-Harness/' : '' ) . 't/multiplexer.t' );
