#!./perl -w

BEGIN {
    chdir "t" if -d "t";
    @INC = qw(. ../lib);
}

# Test srand.

use strict;

require "test.pl";
plan(tests => 4);

# Generate a load of random numbers.
# int() avoids possible floating point error.
sub mk_rand { map int rand 10000, 1..100; }


# Check that rand() is deterministic.
srand(1138);
my @first_run  = mk_rand;

srand(1138);
my @second_run = mk_rand;

ok( eq_array(\@first_run, \@second_run),  'srand(), same arg, same rands' );


# Check that different seeds provide different random numbers
srand(31337);
@first_run  = mk_rand;

srand(1138);
@second_run = mk_rand;

ok( !eq_array(\@first_run, \@second_run),
                                 'srand(), different arg, different rands' );


# Check that srand() isn't affected by $_
{   
    local $_ = 42;
    srand();
    @first_run  = mk_rand;

    srand(42);
    @second_run = mk_rand;

    ok( !eq_array(\@first_run, \@second_run),
                       'srand(), no arg, not affected by $_');
}

# This test checks whether Perl called srand for you.
@first_run  = `$^X -le "print int rand 100 for 1..100"`;
sleep(1); # in case our srand() is too time-dependent
@second_run = `$^X -le "print int rand 100 for 1..100"`;

ok( !eq_array(\@first_run, \@second_run), 'srand() called automatically');
