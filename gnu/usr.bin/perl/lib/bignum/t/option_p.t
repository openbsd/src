#!/usr/bin/perl -w

###############################################################################

use Test;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 2;
  }

use bignum p => '12';

ok (Math::BigInt->precision(),12);
ok (Math::BigFloat->precision(),12);

