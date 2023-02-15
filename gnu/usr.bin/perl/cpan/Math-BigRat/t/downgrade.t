# -*- mode: perl; -*-

# Note that this does not test Math::BigRat upgrading.

use strict;
use warnings;

use Test::More tests => 33;

use Math::BigInt upgrade   => 'Math::BigRat';
use Math::BigRat downgrade => 'Math::BigInt';

is(Math::BigRat->downgrade(), 'Math::BigInt', 'Math::BigRat->downgrade()');
is(Math::BigInt->upgrade(),   'Math::BigRat', 'Math::BigInt->upgrade()');


################################################################################
# Verify that constructors downgrade when they should.

note("Enable downgrading, and see if constructors downgrade");

my $x;

# new()

$x = Math::BigRat -> new("0.5");
cmp_ok($x, "==", 0.5);
is(ref $x, "Math::BigRat", "Creating a 0.5 does not downgrade");

$x = Math::BigRat -> new("4");
cmp_ok($x, "==", 4, 'new("4")');
is(ref $x, "Math::BigInt", "Creating a 4 downgrades to Math::BigInt");

$x = Math::BigRat -> new("0");
cmp_ok($x, "==", 0, 'new("0")');
is(ref $x, "Math::BigInt", "Creating a 0 downgrades to Math::BigInt");

$x = Math::BigRat -> new("1");
cmp_ok($x, "==", 1, 'new("1")');
is(ref $x, "Math::BigInt", "Creating a 1 downgrades to Math::BigInt");

$x = Math::BigRat -> new("Inf");
cmp_ok($x, "==", "Inf", 'new("inf")');
is(ref $x, "Math::BigInt", "Creating an Inf downgrades to Math::BigInt");

$x = Math::BigRat -> new("NaN");
is($x, "NaN", 'new("NaN")');
is(ref $x, "Math::BigInt", "Creating a NaN downgrades to Math::BigInt");

# bzero()

$x = Math::BigRat -> bzero();
cmp_ok($x, "==", 0, "bzero()");
is(ref $x, "Math::BigInt", "Creating a 0 downgrades to Math::BigInt");

# bone()

$x = Math::BigRat -> bone();
cmp_ok($x, "==", 1, "bone()");
is(ref $x, "Math::BigInt", "Creating a 1 downgrades to Math::BigInt");

# binf()

$x = Math::BigRat -> binf();
cmp_ok($x, "==", "Inf", "binf()");
is(ref $x, "Math::BigInt", "Creating an Inf downgrades to Math::BigInt");

# bnan()

$x = Math::BigRat -> bnan();
is($x, "NaN", "bnan()");
is(ref $x, "Math::BigInt", "Creating a NaN downgrades to Math::BigInt");

# from_hex()

$x = Math::BigRat -> from_hex("13a");
cmp_ok($x, "==", 314, 'from_hex("13a")');
is(ref $x, "Math::BigInt", 'from_hex("13a") downgrades to Math::BigInt');

# from_oct()

$x = Math::BigRat -> from_oct("472");
cmp_ok($x, "==", 314, 'from_oct("472")');
is(ref $x, "Math::BigInt", 'from_oct("472") downgrades to Math::BigInt');

# from_bin()

$x = Math::BigRat -> from_bin("100111010");
cmp_ok($x, "==", 314, 'from_bin("100111010")');
is(ref $x, "Math::BigInt",
   'from_bin("100111010") downgrades to Math::BigInt');

note("Disable downgrading, and see if constructors downgrade");

Math::BigRat -> downgrade(undef);

my $half = Math::BigRat -> new("1/2");
my $four = Math::BigRat -> new("4");
my $zero = Math::BigRat -> bzero();
my $inf  = Math::BigRat -> binf();
my $nan  = Math::BigRat -> bnan();

is(ref $half, "Math::BigRat", "Creating a 0.5 does not downgrade");
is(ref $four, "Math::BigRat", "Creating a 4 does not downgrade");
is(ref $zero, "Math::BigRat", "Creating a 0 does not downgrade");
is(ref $inf,  "Math::BigRat", "Creating an Inf does not downgrade");
is(ref $nan,  "Math::BigRat", "Creating a NaN does not downgrade");
