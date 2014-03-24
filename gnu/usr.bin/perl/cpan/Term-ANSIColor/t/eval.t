#!/usr/bin/perl
#
# Test suite for $@ preservation with constants.
#
# Earlier versions of Term::ANSIColor would clobber $@ during AUTOLOAD
# processing and lose its value or leak $@ values to the calling program.
# This is a regression test to ensure that this problem doesn't return.
#
# Copyright 2012 Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

use strict;
use warnings;

use Test::More tests => 5;

# We refer to $@ in the test descriptions.
## no critic (ValuesAndExpressions::RequireInterpolationOfMetachars)

# Load the module.
BEGIN {
    delete $ENV{ANSI_COLORS_ALIASES};
    delete $ENV{ANSI_COLORS_DISABLED};
    use_ok('Term::ANSIColor', qw(:constants));
}

# Ensure that using a constant doesn't leak anything in $@.
is((BOLD 'test'), "\e[1mtest", 'BOLD works');
is($@,            q{},         '... and $@ is empty');

# Store something in $@ and ensure it doesn't get clobbered.
## no critic (BuiltinFunctions::ProhibitStringyEval)
## no critic (ErrorHandling::RequireCheckingReturnValueOfEval)
eval 'sub { syntax';
is((BLINK 'test'), "\e[5mtest", 'BLINK works after eval failure');
isnt($@, q{}, '... and $@ still contains something useful');
