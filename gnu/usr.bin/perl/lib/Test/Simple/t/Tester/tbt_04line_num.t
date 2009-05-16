#!/usr/bin/perl
# $Id: tbt_04line_num.t,v 1.1 2009/05/16 21:42:58 simon Exp $

use Test::More tests => 3;
use Test::Builder::Tester;

is(line_num(),7,"normal line num");
is(line_num(-1),7,"line number minus one");
is(line_num(+2),11,"line number plus two");
