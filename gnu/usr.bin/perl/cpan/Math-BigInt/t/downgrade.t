#!perl

use strict;
use warnings;

use Test::More tests => 15;

use Math::BigInt   upgrade   => 'Math::BigFloat';
use Math::BigFloat downgrade => 'Math::BigInt',
                   upgrade   => 'Math::BigInt';

our ($CLASS, $EXPECTED_CLASS, $LIB);
$CLASS          = "Math::BigInt";
$EXPECTED_CLASS = "Math::BigFloat";
$LIB            = "Math::BigInt::Calc";         # backend

# simplistic test for now
is(Math::BigFloat->downgrade(), 'Math::BigInt', 'Math::BigFloat->downgrade()');
is(Math::BigFloat->upgrade(),   'Math::BigInt', 'Math::BigFloat->upgrade()');

# these downgrade
is(ref(Math::BigFloat->new("inf")), "Math::BigInt",
   qq|ref(Math::BigFloat->new("inf"))|);
is(ref(Math::BigFloat->new("-inf")), "Math::BigInt",
   qq|ref(Math::BigFloat->new("-inf"))|);
is(ref(Math::BigFloat->new("NaN")), "Math::BigInt",
   qq|ref(Math::BigFloat->new("NaN"))|);
is(ref(Math::BigFloat->new("0")), "Math::BigInt",
   qq|ref(Math::BigFloat->new("0"))|);
is(ref(Math::BigFloat->new("1")), "Math::BigInt",
   qq|ref(Math::BigFloat->new("1"))|);
is(ref(Math::BigFloat->new("10")), "Math::BigInt",
   qq|ref(Math::BigFloat->new("10"))|);
is(ref(Math::BigFloat->new("-10")), "Math::BigInt",
   qq|ref(Math::BigFloat->new("-10"))|);
is(ref(Math::BigFloat->new("-10.0E1")), "Math::BigInt",
   qq|ref(Math::BigFloat->new("-10.0E1"))|);

# bug until v1.67:
is(Math::BigFloat->new("0.2E0"), "0.2", qq|Math::BigFloat->new("0.2E0")|);
is(Math::BigFloat->new("0.2E1"), "2",   qq|Math::BigFloat->new("0.2E1")|);
# until v1.67 resulted in 200:
is(Math::BigFloat->new("0.2E2"), "20",  qq|Math::BigFloat->new("0.2E2")|);

# disable, otherwise it screws calculations
Math::BigFloat->upgrade(undef);
is(Math::BigFloat->upgrade() || "", "", qq/Math::BigFloat->upgrade() || ""/);

Math::BigFloat->div_scale(20);  # make it a bit faster
my $x = Math::BigFloat->new(2);    # downgrades
# the following test upgrade for bsqrt() and also makes new() NOT downgrade
# for the bpow() side
is(Math::BigFloat->bpow("2", "0.5"), $x->bsqrt(),
   qq|Math::BigFloat->bpow("2", "0.5")|);

#require 'upgrade.inc'; # all tests here for sharing
