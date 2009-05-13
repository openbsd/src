#!perl -wT
use strict;
use Test::More;

plan skip_all => "Test::Portability::Files required for testing filenames portability"
    unless eval "use Test::Portability::Files; 1";

# run the selected tests
run_tests();
