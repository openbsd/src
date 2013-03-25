#!/usr/bin/perl -w

###############################################################################

use strict;
use Test::More;

eval 'require Math::BigInt::Lite;';
if ($@ eq '')
  {
  plan (tests => 1);
  # can use Lite, so let bignum try it
  require bigrat; bigrat->import();
  # can't get to work a ref(1+1) here, presumable because :constant phase
  # already done
  is ($bigrat::_lite,1);
  }
else
  {
  plan (skip_all =>  "no Math::BigInt::Lite");
  }

