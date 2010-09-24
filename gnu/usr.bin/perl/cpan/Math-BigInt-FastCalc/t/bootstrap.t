#!/usr/bin/perl -w

use Test;
BEGIN 
  {
  $| = 1;
  unshift @INC, '../blib/lib';
  unshift @INC, '../blib/arch';
  unshift @INC, '../lib';
  chdir 't' if -d 't' && !$ENV{PERL_CORE};
  plan tests => 1;
  };

use Math::BigInt::FastCalc;

ok(1); 		# could load it?

