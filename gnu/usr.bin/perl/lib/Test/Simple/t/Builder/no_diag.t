#!/usr/bin/perl -w
# $Id: no_diag.t,v 1.1 2009/05/16 21:42:57 simon Exp $

use Test::More 'no_diag', tests => 2;

pass('foo');
diag('This should not be displayed');

is(Test::More->builder->no_diag, 1);
