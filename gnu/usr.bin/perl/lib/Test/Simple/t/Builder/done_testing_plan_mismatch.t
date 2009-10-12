#!/usr/bin/perl -w

# What if there's a plan and done_testing but they don't match?

use strict;
BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use Test::Builder;
use Test::Builder::NoOutput;

my $tb = Test::Builder::NoOutput->create;

{
    # Normalize test output
    local $ENV{HARNESS_ACTIVE};

    $tb->plan( tests => 3 );
    $tb->ok(1);
    $tb->ok(1);
    $tb->ok(1);

#line 24
    $tb->done_testing(2);
}

my $Test = Test::Builder->new;
$Test->plan( tests => 1 );
$Test->level(0);
$Test->is_eq($tb->read, <<"END");
1..3
ok 1
ok 2
ok 3
not ok 4 - planned to run 3 but done_testing() expects 2
#   Failed test 'planned to run 3 but done_testing() expects 2'
#   at $0 line 24.
END
