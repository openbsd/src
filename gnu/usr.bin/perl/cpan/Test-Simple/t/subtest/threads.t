#!/usr/bin/perl -w

use strict;
use warnings;

use Config;
BEGIN {
    unless ( $] >= 5.008001 && $Config{'useithreads'} && 
             eval { require threads; 'threads'->import; 1; }) 
    {
        print "1..0 # Skip: no working threads\n";
        exit 0;
    }
}

use Test::More;

subtest 'simple test with threads on' => sub {
    is( 1+1, 2,   "simple test" );
    is( "a", "a", "another simple test" );
};

pass("Parent retains sharedness");

done_testing(2);
