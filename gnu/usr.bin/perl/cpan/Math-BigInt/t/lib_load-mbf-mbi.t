# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 10;

use lib "t";

# First load Math::BigFloat with Math::BigInt::Calc.

use Math::BigFloat lib => "Calc";

is(Math::BigFloat -> config("lib"), "Math::BigInt::Calc",
   'Math::BigFloat -> config("lib")');

is(ref Math::BigFloat -> bzero() -> {_m}, "Math::BigInt::Calc",
   'ref Math::BigFloat -> bzero() -> {_m}');

# Math::BigInt will know that we loaded Math::BigInt::Calc.

require Math::BigInt;

is(Math::BigInt -> config("lib"), "Math::BigInt::Calc",
   'Math::BigInt -> config("lib")');

is(ref Math::BigInt -> bzero() -> {value}, "Math::BigInt::Calc",
   "ref Math::BigInt -> bzero() -> {value}");

# Now load Math::BigFloat again with a different lib.

Math::BigFloat -> import(lib => "BareCalc");

is(Math::BigFloat -> config("lib"), "Math::BigInt::Calc",
   'Math::BigFloat -> config("lib")');

is(ref Math::BigFloat -> bzero() -> {_m}, "Math::BigInt::Calc",
   'ref Math::BigFloat -> bzero() -> {_m}');

# See if Math::BigInt knows about Math::BigInt::BareCalc.

is(Math::BigInt -> config("lib"), "Math::BigInt::Calc",
   "Math::BigInt is using library Math::BigInt::Calc");

is(ref Math::BigInt -> bzero() -> {value}, "Math::BigInt::Calc",
   "ref Math::BigInt -> bzero() -> {value}");

# See that Math::BigInt supports "only".

eval { Math::BigInt -> import("only" => "Calc") };
subtest 'Math::BigInt -> import("only" => "Calc")' => sub {
    plan tests => 3;

    is($@, "", '$@ is empty');
    is(Math::BigInt -> config("lib"), "Math::BigInt::Calc",
       'Math::BigInt -> config("lib")');
    is(ref Math::BigInt -> bzero() -> {value}, "Math::BigInt::Calc",
       "ref Math::BigInt -> bzero() -> {value}");
};

# See that Math::BigInt supports "try".

eval { Math::BigInt -> import("try" => "BareCalc") };
subtest 'Math::BigInt -> import("try" => "BareCalc")' => sub {
    plan tests => 3;

    is($@, "", '$@ is empty');
    is(Math::BigInt -> config("lib"), "Math::BigInt::Calc",
       'Math::BigInt -> config("lib")');
    is(ref Math::BigInt -> bzero() -> {value}, "Math::BigInt::Calc",
       "ref Math::BigInt -> bzero() -> {value}");
}
