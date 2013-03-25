#!/usr/bin/perl -w

# test that the new alias names work

use strict;
use Test::More tests => 6;

BEGIN { unshift @INC, 't'; }

use Math::BigInt::Subclass;

use vars qw/$CL $x/;
$CL = 'Math::BigInt::Subclass';

require 't/alias.inc';
