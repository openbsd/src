#!/usr/bin/perl -w

# <<<Fill in with what this test does.>>>
# Copy this when writing new tests to avoid forgetting the core boilerplate

# Magic for core
BEGIN {
    # Always run in t to unify behavor with core
    chdir 't' if -d 't';
}

# Use things from t/lib/
use lib './lib';
use strict;
use ExtUtils::MakeMaker;

use Test::More;

pass("Your test code goes here");

done_testing();
