#!/usr/bin/perl -Tw

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

use Test::More tests => 7;

use_ok( 'Test::Harness::Assert' );


ok( defined &assert,                'assert() exported' );

ok( !eval { assert( 0 ); 1 },       'assert( FALSE ) causes death' );
like( $@, '/Assert failed/',        '  with the right message' );

ok( eval { assert( 1 );  1 },       'assert( TRUE ) does nothing' );

ok( !eval { assert( 0, 'some name' ); 1 },  'assert( FALSE, NAME )' );
like( $@, '/some name/',                    '  has the name' );
