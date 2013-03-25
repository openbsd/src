#!perl -w

use strict;

use POSIX;
use Test::More;

# These tests are mainly to make sure that these arithmetic functions
# exist and are accessible.  They are not meant to be an exhaustive
# test for the interface.

sub between {
    my ($low, $have, $high, $desc) = @_;
    local $Test::Builder::Level = $Test::Builder::Level + 1;

    cmp_ok($have, '>=', $low, $desc);
    cmp_ok($have, '<=', $high, $desc);
}

is(acos(1), 0, "Basic acos(1) test");
between(3.14, acos(-1), 3.15, 'acos(-1)');
between(1.57, acos(0), 1.58, 'acos(0)');
is(asin(0), 0, "Basic asin(0) test");
cmp_ok(asin(1), '>', 1.57, "Basic asin(1) test");
cmp_ok(asin(-1), '<', -1.57, "Basic asin(-1) test");
cmp_ok(asin(1), '==', -asin(-1), 'asin(1) == -asin(-1)');
is(atan(0), 0, "Basic atan(0) test");
between(0.785, atan(1), 0.786, 'atan(1)');
between(-0.786, atan(-1), -0.785, 'atan(-1)');
cmp_ok(atan(1), '==', -atan(-1), 'atan(1) == -atan(-1)');
is(cosh(0), 1, "Basic cosh(0) test");  
between(1.54, cosh(1), 1.55, 'cosh(1)');
between(1.54, cosh(-1), 1.55, 'cosh(-1)');
is(cosh(1), cosh(-1), 'cosh(1) == cosh(-1)');
is(floor(1.23441242), 1, "Basic floor(1.23441242) test");
is(floor(-1.23441242), -2, "Basic floor(-1.23441242) test");
is(fmod(3.5, 2.0), 1.5, "Basic fmod(3.5, 2.0) test");
is(join(" ", frexp(1)), "0.5 1",  "Basic frexp(1) test");
is(ldexp(0,1), 0, "Basic ldexp(0,1) test");
is(log10(1), 0, "Basic log10(1) test"); 
is(log10(10), 1, "Basic log10(10) test");
is(join(" ", modf(1.76)), "0.76 1", "Basic modf(1.76) test");
is(sinh(0), 0, "Basic sinh(0) test"); 
between(1.17, sinh(1), 1.18, 'sinh(1)');
between(-1.18, sinh(-1), -1.17, 'sinh(-1)');
is(tan(0), 0, "Basic tan(0) test");
between(1.55, tan(1), 1.56, 'tan(1)');
between(1.55, tan(1), 1.56, 'tan(-1)');
cmp_ok(tan(1), '==', -tan(-1), 'tan(1) == -tan(-1)');
is(tanh(0), 0, "Basic tanh(0) test"); 
between(0.76, tanh(1), 0.77, 'tanh(1)');
between(-0.77, tanh(-1), -0.76, 'tanh(-1)');
cmp_ok(tanh(1), '==', -tanh(-1), 'tanh(1) == -tanh(-1)');

done_testing();
