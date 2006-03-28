#!/usr/bin/perl -w

use Test::More 'no_diag', tests => 2;

pass('foo');
diag('This should not be displayed');

is(Test::More->builder->no_diag, 1);
