#!/usr/bin/perl -w

# test that the new alias names work

use strict;
use Test::More tests => 6;

use Math::BigFloat;

use vars qw/$x $CL/;

$CL = 'Math::BigFloat';

require 't/alias.inc';
