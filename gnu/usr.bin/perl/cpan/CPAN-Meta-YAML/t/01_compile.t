#!/usr/bin/perl

# Load testing for CPAN::Meta::YAML

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use File::Spec::Functions ':ALL';
use Test::More tests => 3;

# Check their perl version
ok( $] >= 5.004, "Your perl is new enough" );

# Does the module load
use_ok( 'CPAN::Meta::YAML'   );
use_ok( 't::lib::Test' );
