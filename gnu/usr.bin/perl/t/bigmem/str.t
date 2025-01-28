#!perl
BEGIN {
    chdir 't' if -d 't';
    @INC = "../lib";
    require './test.pl';
}

use Config qw(%Config);

# 2G each for the $p2g, $n2g and $t

$ENV{PERL_TEST_MEMORY} >= 11
    or skip_all("Need ~11Gb for this test");
$Config{ptrsize} >= 8
    or skip_all("Need 64-bit pointers for this test");

my $p = "A";
my $n = ~$p;

my $sz = 0x8000_0001;

my $p2g = ($p x $sz);

is(length $p2g, $sz, "check p2g size");

my $t = ($p x $sz);
ok($t eq $p2g, "check scalar repeat with large count");
undef $t;
my $two = 2; # no constant folding

$t = ($p2g x $two);
ok($t eq "$p2g$p2g", "check scalar repeat with large source");
undef $t;

$t = ~$p2g;
my $n2g = ($n x $sz);

is(length $n2g, $sz, "check p2g size");

# don't risk a 4GB diagnostic if they don't match
ok($t eq $n2g, "string complement very large string");

done_testing();

