#!/usr/bin/perl -w

###############################################################################

use Test::More;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 4;
  }

use bignum a => '12';

my @C = qw/Math::BigInt Math::BigFloat/;

foreach my $c (@C)
  {
  is ($c->accuracy(),12, "$c accuracy = 12");
  }

bignum->import( accuracy => '23');

foreach my $c (@C)
  {
  is ($c->accuracy(), 23, "$c accuracy = 23");
  }

