#!perl -w

use strict;

use POSIX;
use Test::More tests => 14;

# These tests are mainly to make sure that these arithmatic functions
# exist and are accessible.  They are not meant to be an exhaustive
# test for the interface.

is(acos(1), 0, "Basic acos(1) test");
is(asin(0), 0, "Basic asin(0) test");
is(atan(0), 0, "Basic atan(0) test");
is(cosh(0), 1, "Basic cosh(0) test");  
is(floor(1.23441242), 1, "Basic floor(1.23441242) test");
is(fmod(3.5, 2.0), 1.5, "Basic fmod(3.5, 2.0) test");
is(join(" ", frexp(1)), "0.5 1",  "Basic frexp(1) test");
is(ldexp(0,1), 0, "Basic ldexp(0,1) test");
is(log10(1), 0, "Basic log10(1) test"); 
is(log10(10), 1, "Basic log10(10) test");
is(join(" ", modf(1.76)), "0.76 1", "Basic modf(1.76) test");
is(sinh(0), 0, "Basic sinh(0) test"); 
is(tan(0), 0, "Basic tan(0) test");
is(tanh(0), 0, "Basic tanh(0) test"); 
