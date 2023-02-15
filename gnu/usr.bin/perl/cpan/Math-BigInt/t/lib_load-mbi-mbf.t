# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 10;

use lib "t";

# First load Math::BigInt with Math::BigInt::Calc.

use Math::BigInt lib => "Calc";

is(Math::BigInt -> config("lib"), "Math::BigInt::Calc",
   'Math::BigInt -> config("lib")');

is(ref Math::BigInt -> bzero() -> {value}, "Math::BigInt::Calc",
   'ref Math::BigInt -> bzero() -> {value}');

# Math::BigFloat will know that we loaded Math::BigInt::Calc.

require Math::BigFloat;

is(Math::BigFloat -> config("lib"), "Math::BigInt::Calc",
   'Math::BigFloat -> config("lib")');

is(ref Math::BigFloat -> bzero() -> {_m}, "Math::BigInt::Calc",
   "ref Math::BigFloat -> bzero() -> {_m}");

# Now load Math::BigInt again with a different lib.

Math::BigInt -> import(lib => "BareCalc");

is(Math::BigInt -> config("lib"), "Math::BigInt::Calc",
   'Math::BigInt -> config("lib")');

is(ref Math::BigInt -> bzero() -> {value}, "Math::BigInt::Calc",
   'ref Math::BigInt -> bzero() -> {value}');

# See if Math::BigFloat knows about Math::BigInt::BareCalc.

is(Math::BigFloat -> config("lib"), "Math::BigInt::Calc",
   "Math::BigFloat is using library Math::BigInt::Calc");

is(ref Math::BigFloat -> bzero() -> {_m}, "Math::BigInt::Calc",
   "ref Math::BigFloat -> bzero() -> {_m}");

# See that Math::BigFloat supports "only".

eval { Math::BigFloat -> import("only" => "Calc") };
subtest 'Math::BigFloat -> import("only" => "Calc")' => sub {
    plan tests => 3;

    is($@, "", '$@ is empty');
    is(Math::BigFloat -> config("lib"), "Math::BigInt::Calc",
       'Math::BigFloat -> config("lib")');
    is(ref Math::BigFloat -> bzero() -> {_m}, "Math::BigInt::Calc",
       "ref Math::BigFloat -> bzero() -> {_m}");
};

# See that Math::BigFloat supports "try".

eval { Math::BigFloat -> import("try" => "BareCalc") };
subtest 'Math::BigFloat -> import("try" => "BareCalc")' => sub {
    plan tests => 3;

    is($@, "", '$@ is empty');
    is(Math::BigFloat -> config("lib"), "Math::BigInt::Calc",
       'Math::BigFloat -> config("lib")');
    is(ref Math::BigFloat -> bzero() -> {_m}, "Math::BigInt::Calc",
       "ref Math::BigFloat -> bzero() -> {_m}");
}
