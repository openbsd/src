# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 10;

use Math::BigFloat;
use Scalar::Util qw< refaddr >;

my $x;

################################################################################

note('class method');

# When no accuracy is specified, default accuracy shall be used.

$x = Math::BigFloat -> bpi();
is($x, '3.141592653589793238462643383279502884197',
   '$x = Math::BigFloat -> bpi();');
is(ref($x), "Math::BigFloat", '$x is a Math::BigFloat');

# When accuracy is specified, it shall be used.

$x = Math::BigFloat -> bpi(10);
is($x, '3.141592654',
   '$x = Math::BigFloat -> bpi(10);');
is(ref($x), "Math::BigFloat", '$x is a Math::BigFloat');

################################################################################

note('instance method');

my $y;

# When no accuracy is specified, default accuracy shall be used.

$x = Math::BigFloat -> new(100);
$y = $x -> bpi();
is($x, '3.141592653589793238462643383279502884197',
   '$x = Math::BigFloat -> new(100); $y = $x -> bpi();');
is(ref($x), "Math::BigFloat", '$x is a Math::BigFloat');
is(refaddr($x), refaddr($y), '$x and $y are the same object');

# When accuracy is specified, it shall be used.

$x = Math::BigFloat -> new(100);
$y = $x -> bpi(10);
is($x, '3.141592654',
   '$x = Math::BigFloat -> new(100); $y = $x -> bpi(10);');
is(ref($x), "Math::BigFloat", '$x is a Math::BigFloat');
is(refaddr($x), refaddr($y), '$x and $y are the same object');
