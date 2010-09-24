#!/usr/bin/perl -w

# test that overloaded compare works when NaN are involved

use strict;
use Test::More;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';	# for running manually
  plan tests => 26;
  }

use Math::BigInt;
use Math::BigFloat;

compare (Math::BigInt->bnan(),   Math::BigInt->bone() );
compare (Math::BigFloat->bnan(), Math::BigFloat->bone() );

sub compare
  {
  my ($nan, $one) = @_;

  is ($one, $one, '1 == 1');

  is ($one != $nan, 1, "1 != NaN");
  is ($nan != $one, 1, "NaN != 1");
  is ($nan != $nan, 1, "NaN != NaN");

  is ($nan == $one, '', "NaN == 1");
  is ($one == $nan, '', "1 == NaN");
  is ($nan == $nan, '', "NaN == NaN");

  is ($nan <= $one, '', "NaN <= 1");
  is ($one <= $nan, '', "1 <= NaN");
  is ($nan <= $nan, '', "NaN <= NaN");

  is ($nan >= $one, '', "NaN >= 1");
  is ($one >= $nan, '', "1 >= NaN");
  is ($nan >= $nan, '', "NaN >= NaN");
  }

