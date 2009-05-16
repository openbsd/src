#!/usr/bin/perl -w
# $Id: curr_test.t,v 1.1 2009/05/16 21:42:57 simon Exp $

# Dave Rolsky found a bug where if current_test() is used and no
# tests are run via Test::Builder it will blow up.

use Test::Builder;
$TB = Test::Builder->new;
$TB->plan(tests => 2);
print "ok 1\n";
print "ok 2\n";
$TB->current_test(2);
