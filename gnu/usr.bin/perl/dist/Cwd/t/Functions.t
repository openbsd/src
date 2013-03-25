#!/usr/bin/perl -w

use Test::More tests => 3;
BEGIN {use_ok('File::Spec::Functions', ':ALL');}

is(catfile('a','b','c'), File::Spec->catfile('a','b','c'));

# seems to return 0 or 1, so see if we can call it - 2003-07-07 tels
like(case_tolerant(), qr/\A(?:0|1)\z/);
