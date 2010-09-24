#!/usr/bin/perl -w

###############################################################################

use Test::More;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 7;
  }

my @C = qw/Math::BigInt Math::BigFloat Math::BigRat/;

# bigrat (bug until v0.15)
use bigrat a => 2;

foreach my $c (@C)
  {
  is ($c->accuracy(), 2, "$c accuracy = 2");
  }

eval { bigrat->import( accuracy => '42') };

is ($@, '', 'no error');

foreach my $c (@C)
  {
  is ($c->accuracy(), 42, "$c accuracy = 42");
  }

