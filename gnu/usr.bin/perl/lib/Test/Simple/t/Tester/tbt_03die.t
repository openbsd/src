#!/usr/bin/perl
# $Id: tbt_03die.t,v 1.1 2009/05/16 21:42:58 simon Exp $

use Test::Builder::Tester tests => 1;
use Test::More;

eval {
    test_test("foo");
};
like($@,
     "/Not testing\.  You must declare output with a test function first\./",
     "dies correctly on error");

