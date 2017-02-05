#!perl

###############################################################################

use strict;
use warnings;

use Test::More tests => 4;

use bignum a => '12';

my @CLASSES = qw/Math::BigInt Math::BigFloat/;

foreach my $class (@CLASSES) {
    is($class->accuracy(),12, "$class accuracy = 12");
}

bignum->import(accuracy => '23');

foreach my $class (@CLASSES) {
    is($class->accuracy(), 23, "$class accuracy = 23");
}
