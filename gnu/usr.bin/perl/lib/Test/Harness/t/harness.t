#!/usr/bin/perl -Tw

BEGIN {
    if ( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;

use Test::More tests => 2;

BEGIN {
    use_ok( 'Test::Harness' );
}

my $strap = Test::Harness->strap;
isa_ok( $strap, 'Test::Harness::Straps' );
