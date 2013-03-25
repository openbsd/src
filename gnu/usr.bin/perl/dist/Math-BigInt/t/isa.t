#!/usr/bin/perl -w

use strict;
use Test::More tests => 7;

BEGIN { unshift @INC, 't'; }

use Math::BigInt::Subclass;
use Math::BigFloat::Subclass;
use Math::BigInt;
use Math::BigFloat;

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigInt::Subclass";
$CL = "Math::BigInt::Calc";

# Check that a subclass is still considered a BigInt
isa_ok ($class->new(123), 'Math::BigInt');

# ditto for plain Math::BigInt
isa_ok (Math::BigInt->new(123), 'Math::BigInt');

# But Math::BigFloats aren't
isnt (Math::BigFloat->new(123)->isa('Math::BigInt'), 1);

# see what happens if we feed a Math::BigFloat into new()
$x = Math::BigInt->new(Math::BigFloat->new(123));
is (ref($x),'Math::BigInt');
isa_ok ($x, 'Math::BigInt');

# ditto for subclass
$x = Math::BigInt->new(Math::BigFloat->new(123));
is (ref($x),'Math::BigInt');
isa_ok ($x, 'Math::BigInt');
