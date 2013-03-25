#!perl -w

use strict;
use utf8;
use Test::More 'no_plan';

use_ok('XS::APItest');

*hint_exists = *hint_exists = \&XS::APItest::Hash::refcounted_he_exists;
*hint_fetch = *hint_fetch = \&XS::APItest::Hash::refcounted_he_fetch;

require '../../t/op/caller.pl';
