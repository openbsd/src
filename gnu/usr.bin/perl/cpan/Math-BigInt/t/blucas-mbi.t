# -*- mode: perl; -*-

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
$y = $x -> blucas();
is($y, "-15127", "blucas(-20)");

$x = Math::BigInt -> new("-15");
$y = $x -> blucas();
is($y, "1364", "blucas(-15)");

$x = Math::BigInt -> new("-2");
$y = $x -> blucas();
is($y, "-3", "blucas(-2)");

$x = Math::BigInt -> new("-1");
$y = $x -> blucas();
is($y, "1", "blucas(-1)");

$x = Math::BigInt -> new("0");
$y = $x -> blucas();
is($y, "2", "blucas(0)");

$x = Math::BigInt -> new("1");
$y = $x -> blucas();
is($y, "1", "blucas(1)");

$x = Math::BigInt -> new("2");
$y = $x -> blucas();
is($y, "3", "blucas(2)");

$x = Math::BigInt -> new("15");
$y = $x -> blucas();
is($y, "1364", "blucas(15)");

$x = Math::BigInt -> new("20");
$y = $x -> blucas();
is($y, "15127", "blucas(20)");

$x = Math::BigInt -> new("250");
$y = $x -> blucas();
is($y, "17656721319717734662791328845675730903632844218828123", "blucas(250)");

# Infinites and NaN.

$x = Math::BigInt -> binf("+");
$y = $x -> blucas();
is($y, "inf", "blucas(+inf)");

$x = Math::BigInt -> binf("-");
$y = $x -> blucas();
is($y, "NaN", "blucas(-inf)");

$x = Math::BigInt -> bnan();
$y = $x -> blucas();
is($y, "NaN", "blucas(NaN)");

###############################################################################
# List context.
###############################################################################

my @y;

$x = Math::BigInt -> new("10");
@y = $x -> blucas();
is_deeply(\@y, [2, 1, 3, 4, 7, 11, 18, 29, 47, 76, 123], "blucas(10)");

$x = Math::BigInt -> new("-10");
@y = $x -> blucas();
is_deeply(\@y, [2, 1, -3, 4, -7, 11, -18, 29, -47, 76, -123], "blucas(-10)");
