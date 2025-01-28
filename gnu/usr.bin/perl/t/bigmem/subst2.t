#!perl
BEGIN {
    chdir 't' if -d 't';
    @INC = "../lib";
    require './test.pl';
}

use Config qw(%Config);

$ENV{PERL_TEST_MEMORY} >= 8
    or skip_all("Need ~8Gb for this test");
$Config{ptrsize} >= 8
    or skip_all("Need 64-bit pointers for this test");

my $x = "A";
my $y = "B";

study($r);
my $r = ($x x 0x8000_0001) . ($y x 10) . ($x x 5);

my $c = "C";

$r =~ s/B/ $c++ /ge;

is(substr($r, -10), "HIJKLAAAAA", "rxres_restore I32");

done_testing();
