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

use Test::Harness;
use Test::More tests => 1;
use Dev::Null;

push @INC, 'we_added_this_lib';

tie *NULL, 'Dev::Null' or die $!;
select NULL;
my($tot, $failed) = Test::Harness::_run_all_tests(
    $ENV{PERL_CORE}
    ? 'lib/sample-tests/inc_taint'
    : 't/sample-tests/inc_taint'
);
select STDOUT;

ok( Test::Harness::_all_ok($tot), 'tests with taint on preserve @INC' );
