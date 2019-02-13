#!perl

use strict;
use warnings;

use Test::More tests => 15;

use Math::BigInt;

my $x;

###############################################################################
# Scalar context.
###############################################################################

my $y;

# Finite numbers.

$x = Math::BigInt -> new("-20");
$y = $x -> bfib();
is($y, "-6765", "bfib(-20)");

$x = Math::BigInt -> new("-15");
$y = $x -> bfib();
is($y, "610", "bfib(-15)");

$x = Math::BigInt -> new("-2");
$y = $x -> bfib();
is($y, "-1", "bfib(-2)");

$x = Math::BigInt -> new("-1");
$y = $x -> bfib();
is($y, "1", "bfib(-1)");

$x = Math::BigInt -> new("0");
$y = $x -> bfib();
is($y, "0", "bfib(0)");

$x = Math::BigInt -> new("1");
$y = $x -> bfib();
is($y, "1", "bfib(1)");

$x = Math::BigInt -> new("2");
$y = $x -> bfib();
is($y, "1", "bfib(2)");

$x = Math::BigInt -> new("15");
$y = $x -> bfib();
is($y, "610", "bfib(15)");

$x = Math::BigInt -> new("20");
$y = $x -> bfib();
is($y, "6765", "bfib(20)");

$x = Math::BigInt -> new("250");
$y = $x -> bfib();
is($y, "7896325826131730509282738943634332893686268675876375", "bfib(250)");

# Infinites and NaN.

$x = Math::BigInt -> binf("+");
$y = $x -> bfib();
is($y, "inf", "bfib(+inf)");

$x = Math::BigInt -> binf("-");
$y = $x -> bfib();
is($y, "NaN", "bfib(-inf)");

$x = Math::BigInt -> bnan();
$y = $x -> bfib();
is($y, "NaN", "bfib(NaN)");

###############################################################################
# List context.
###############################################################################

my @y;

$x = Math::BigInt -> new("10");
@y = $x -> bfib();
is_deeply(\@y, [0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55], "bfib(10)");

$x = Math::BigInt -> new("-10");
@y = $x -> bfib();
is_deeply(\@y, [0, 1, -1, 2, -3, 5, -8, 13, -21, 34, -55], "bfib(-10)");
