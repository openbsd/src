#!/usr/bin/perl -w

# Test bpi() and bexp()

use strict;
use Test::More tests => 8;

use Math::BigFloat;

#############################################################################

my $pi = Math::BigFloat::bpi();

ok (!exists $pi->{_a}, 'A not set');
ok (!exists $pi->{_p}, 'P not set');

$pi = Math::BigFloat->bpi();

ok (!exists $pi->{_a}, 'A not set');
ok (!exists $pi->{_p}, 'P not set');

$pi = Math::BigFloat->bpi(10);

is ($pi->{_a}, 10, 'A set');
is ($pi->{_p}, undef, 'P not set');

#############################################################################
my $e = Math::BigFloat->new(1)->bexp();

ok (!exists $e->{_a}, 'A not set');
ok (!exists $e->{_p}, 'P not set');


