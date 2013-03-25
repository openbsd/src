#!/usr/bin/perl
use strict;
use warnings;
$| = 1;
use Carp;
use Cwd;
use File::Spec;
use File::Temp qw( tempdir );
use Test::More qw(no_plan); # tests =>  7;
use lib qw( lib );
use ExtUtils::ParseXS::Utilities qw(
    analyze_preprocessor_statements
);

#      ( $self, $XSS_work_idx, $BootCode_ref ) =
#        analyze_preprocessor_statements(
#          $self, $statement, $XSS_work_idx, $BootCode_ref
#        );

pass("Passed all tests in $0");


