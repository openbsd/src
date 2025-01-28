#!perl
BEGIN {
    chdir 't' if -d 't';
    @INC = "../lib";
    require './test.pl';
}

use strict;
use Config qw(%Config);
use XS::APItest;

# memory usage checked with top
$ENV{PERL_TEST_MEMORY} >= 17
    or skip_all("Need ~17GB for this test");
$Config{ptrsize} >= 8
    or skip_all("Need 64-bit pointers for this test");
# this tests what happens when we don't have wide marks
XS::APItest::wide_marks()
    and skip_all("Configured for SSize_t marks");

my @x;
$x[0x8000_0000] = "Hello";

sub x { @x }

ok(!eval { () = x(); 1 }, "stack overflow");
done_testing();
