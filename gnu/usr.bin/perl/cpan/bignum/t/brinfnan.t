#!/usr/bin/perl -w

###############################################################################

use Test;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  unshift @INC, '../lib/bignum/t' if $ENV{PERL_CORE};
  plan tests => 26;
  }

use bigrat;

my ($x);

require "infnan.inc";

