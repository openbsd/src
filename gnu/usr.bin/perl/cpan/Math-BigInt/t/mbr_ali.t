# -*- mode: perl; -*-

# test that the new alias names work

use strict;
use warnings;

use Test::More tests => 6;

use Math::BigRat;

our $CLASS;
$CLASS = 'Math::BigRat';

require './t/alias.inc';
