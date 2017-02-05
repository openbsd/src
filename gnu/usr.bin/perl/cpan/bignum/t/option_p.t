#!perl

use strict;
use warnings;

use Test::More tests => 4;

my @CLASSES = qw/Math::BigInt Math::BigFloat/;

use bignum p => '12';

foreach my $class (@CLASSES) {
    is($class->precision(), 12, "$class precision = 12");
}

bignum->import(p => '42');

foreach my $class (@CLASSES) {
    is($class->precision(), 42, "$class precision = 42");
}
