#!/usr/bin/perl -w

use strict;
use Test::More tests => 4;

my @C = qw/Math::BigInt Math::BigFloat/;

use bignum p => '12';

foreach my $c (@C)
  {
  is ($c->precision(),12, "$c precision = 12");
  }

bignum->import( p => '42' );

foreach my $c (@C)
  {
  is ($c->precision(),42, "$c precision = 42");
  }

