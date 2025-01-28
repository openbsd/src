# -*- mode: perl; -*-

# Test bpi() and bexp()

use strict;
use warnings;

use Test::More tests => 8;

use Math::BigFloat;

#############################################################################

my $pi = Math::BigFloat::bpi();

is($pi->{accuracy}, undef, 'A is not defined');
is($pi->{precision}, undef, 'P is not defined');

$pi = Math::BigFloat->bpi();

is($pi->{accuracy}, undef, 'A is not defined');
is($pi->{precision}, undef, 'P is not defined');

$pi = Math::BigFloat->bpi(10);

is($pi->{accuracy}, 10,    'A is defined');
is($pi->{precision}, undef, 'P is not defined');

#############################################################################

my $e = Math::BigFloat->new(1)->bexp();

is($e->{accuracy}, undef, 'A is not defined');
is($e->{precision}, undef, 'P is not defined');
