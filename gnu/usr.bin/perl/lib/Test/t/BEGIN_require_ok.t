#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use Test::More;

my $result;
BEGIN {
    eval {
        require_ok("Wibble");
    };
    $result = $@;
}

plan tests => 1;
like $result, '/^You tried to run a test without a plan/';
