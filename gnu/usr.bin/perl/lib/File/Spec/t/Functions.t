#!/usr/bin/perl -w

use Test;
use File::Spec::Functions qw/:ALL/;
plan tests => 2;

ok catfile('a','b','c'), File::Spec->catfile('a','b','c');

# seems to return 0 or 1, so see if we can call it - 2003-07-07 tels
ok case_tolerant(), '/^0|1$/';
