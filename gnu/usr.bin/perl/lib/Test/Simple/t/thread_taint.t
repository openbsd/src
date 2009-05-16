#!/usr/bin/perl -w
# $Id: thread_taint.t,v 1.2 2009/05/16 21:42:57 simon Exp $

use Test::More tests => 1;

ok( !$INC{'threads.pm'}, 'Loading Test::More does not load threads.pm' );
