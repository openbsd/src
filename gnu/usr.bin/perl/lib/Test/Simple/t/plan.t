#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

use Test::More;

plan tests => 4;
eval { plan tests => 4 };
like( $@, '/^You tried to plan twice!/',    'disallow double plan' );
eval { plan 'no_plan'  };
like( $@, '/^You tried to plan twice!/',    'disallow chaning plan' );

pass('Just testing plan()');
pass('Testing it some more');
