#!/usr/bin/perl -w

use Test::More;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 4;
  }

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

