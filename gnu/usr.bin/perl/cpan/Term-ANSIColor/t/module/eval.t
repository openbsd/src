#!/usr/bin/perl
#
# Test suite for $@ preservation with constants.
#
# Earlier versions of Term::ANSIColor would clobber $@ during AUTOLOAD
# processing and lose its value or leak $@ values to the calling program.
# This is a regression test to ensure that this problem doesn't return.
#
# Copyright 2012, 2013, 2014 Russ Allbery <rra@cpan.org>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

use strict;
use warnings;

use Test::More tests => 15;

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
eval 'sub { syntax';
is((BLINK 'test'), "\e[5mtest", 'BLINK works after eval failure');
isnt($@, q{}, '... and $@ still contains something useful');

# Do some additional unnecessary testing so that coverage analysis works
# properly.  First, check disabled colors.
local $ENV{ANSI_COLORS_DISABLED} = 1;
is(BOLD,  q{}, 'ANSI_COLORS_DISABLED works for BOLD');
is(BLINK, q{}, '...and for BLINK');
delete $ENV{ANSI_COLORS_DISABLED};

# Now, AUTORESET.
$Term::ANSIColor::AUTORESET = 1;
is((BOLD 't'),  "\e[1mt\e[0m", 'AUTORESET works for BOLD');
is((BLINK 't'), "\e[5mt\e[0m", '...and for BLINK');
is((BOLD),      "\e[1m",       'AUTORESET without text for BOLD');
is((BLINK),     "\e[5m",       '...and for BLINK');
$Term::ANSIColor::AUTORESET = 0;

# And, finally, AUTOLOCAL.
$Term::ANSIColor::AUTOLOCAL = 1;
is((BOLD 't'),  "\e[1mt\e[0m", 'AUTOLOCAL works for BOLD');
is((BLINK 't'), "\e[5mt\e[0m", '...and for BLINK');
is((BOLD),      "\e[1m",       'AUTOLOCAL without text for BOLD');
is((BLINK),     "\e[5m",       '...and for BLINK');
$Term::ANSIColor::AUTOLOCAL = 0;
