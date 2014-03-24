#!perl

use Test::More tests => 1;

# Make sure that warnings are only enabled if we enable them
# specifically.
ok !$^W, 'warnings disabled';

# vim:ts=2:sw=2:et:ft=perl

