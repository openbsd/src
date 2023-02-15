# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 48;

use Math::BigInt;

my $x;
my $y;

###############################################################################
# bdigitsum()

# Finite numbers.

$x = Math::BigInt -> new("123");
isa_ok($x, 'Math::BigInt');
$y = $x -> bdigitsum();
isa_ok($y, 'Math::BigInt');
is($x, "6");
is($y, "6");

$x = Math::BigInt -> new("0");
isa_ok($x, 'Math::BigInt');
$y = $x -> bdigitsum();
isa_ok($y, 'Math::BigInt');
is($x, "0");
is($y, "0");

$x = Math::BigInt -> new("-123");
isa_ok($x, 'Math::BigInt');
$y = $x -> bdigitsum();
isa_ok($y, 'Math::BigInt');
is($x, "6");
is($y, "6");

# Infinity

$x = Math::BigInt -> binf("+");
isa_ok($x, 'Math::BigInt');
$y = $x -> bdigitsum();
isa_ok($y, 'Math::BigInt');
is($x, "NaN");
is($y, "NaN");

$x = Math::BigInt -> binf("-");
isa_ok($x, 'Math::BigInt');
$y = $x -> bdigitsum();
isa_ok($y, 'Math::BigInt');
is($x, "NaN");
is($y, "NaN");

# NaN

$x = Math::BigInt -> bnan();
isa_ok($x, 'Math::BigInt');
$y = $x -> bdigitsum();
isa_ok($y, 'Math::BigInt');
is($x, "NaN");
is($y, "NaN");

###############################################################################
# digitsum()

# Finite numbers.

$x = Math::BigInt -> new("123");
isa_ok($x, 'Math::BigInt');
$y = $x -> digitsum();
isa_ok($y, 'Math::BigInt');
is($x, "123");
is($y, "6");

$x = Math::BigInt -> new("0");
isa_ok($x, 'Math::BigInt');
$y = $x -> digitsum();
isa_ok($y, 'Math::BigInt');
is($x, "0");
is($y, "0");

$x = Math::BigInt -> new("-123");
isa_ok($x, 'Math::BigInt');
$y = $x -> digitsum();
isa_ok($y, 'Math::BigInt');
is($x, "-123");
is($y, "6");

# Infinity

$x = Math::BigInt -> binf("+");
isa_ok($x, 'Math::BigInt');
$y = $x -> digitsum();
isa_ok($y, 'Math::BigInt');
is($x, "inf");
is($y, "NaN");

$x = Math::BigInt -> binf("-");
isa_ok($x, 'Math::BigInt');
$y = $x -> digitsum();
isa_ok($y, 'Math::BigInt');
is($x, "-inf");
is($y, "NaN");

# NaN

$x = Math::BigInt -> bnan();
isa_ok($x, 'Math::BigInt');
$y = $x -> digitsum();
isa_ok($y, 'Math::BigInt');
is($x, "NaN");
is($y, "NaN");
