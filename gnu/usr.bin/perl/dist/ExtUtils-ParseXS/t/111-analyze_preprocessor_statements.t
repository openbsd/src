#!/usr/bin/perl
use strict;
use warnings;
$| = 1;
use Test::More qw(no_plan); # tests =>  7;
use ExtUtils::ParseXS::Utilities qw(
    analyze_preprocessor_statement
);

# XXX not yet tested
# $self->analyze_preprocessor_statement($statement);

pass("Passed all tests in $0");


