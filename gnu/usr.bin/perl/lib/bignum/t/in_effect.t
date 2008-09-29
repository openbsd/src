#!/usr/bin/perl -w

###############################################################################
# Test in_effect()

use Test::More;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 9;
  }

use bigint;
use bignum;
use bigrat;

can_ok ('bigint', qw/in_effect/);
can_ok ('bignum', qw/in_effect/);
can_ok ('bigrat', qw/in_effect/);

SKIP: {
  skip ('Need at least Perl v5.9.4', 3) if $] < "5.009005";

  is (bigint::in_effect(), 1, 'bigint in effect');
  is (bignum::in_effect(), 1, 'bignum in effect');
  is (bigrat::in_effect(), 1, 'bigrat in effect');
  }

{
  no bigint;
  no bignum;
  no bigrat;

  is (bigint::in_effect(), undef, 'bigint not in effect');
  is (bignum::in_effect(), undef, 'bignum not in effect');
  is (bigrat::in_effect(), undef, 'bigrat not in effect');
}

