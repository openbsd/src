#!/usr/bin/perl -w

###############################################################################

use Test;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 4;
  }

use bignum a => '12';

ok (Math::BigInt->accuracy(),12);
ok (Math::BigFloat->accuracy(),12);

bignum->import( accuracy => '23');

ok (Math::BigInt->accuracy(),23);
ok (Math::BigFloat->accuracy(),23);

