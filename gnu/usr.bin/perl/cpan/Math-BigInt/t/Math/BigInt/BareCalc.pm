#!perl

package Math::BigInt::BareCalc;

use strict;
use warnings;

our $VERSION = '1.999803';

# Package to to test Bigint's simulation of Calc

use Math::BigInt::Calc '1.9998';
our @ISA = qw(Math::BigInt::Calc);

print "# BareCalc using Calc v", Math::BigInt::Calc -> VERSION, "\n";

1;
