#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

use Test::More;

BEGIN {
    if( !$ENV{HARNESS_ACTIVE} && $ENV{PERL_CORE} ) {
        plan skip_all => "Won't work with t/TEST";
    }
}

BEGIN {
    require Test::Harness;
}

if( $Test::Harness::VERSION < 1.20 ) {
    plan skip_all => 'Need Test::Harness 1.20 or up';
}

use strict;
use Test::Builder;

plan 'no_plan';
is(Test::Builder->has_plan, 'no_plan', 'has no_plan');
