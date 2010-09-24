#!/usr/bin/perl

# Load testing for YAML::Tiny

BEGIN {
	if( $ENV{PERL_CORE} ) {
		chdir 't';
		@INC = ('../lib', 'lib');
	}
	else {
		unshift @INC, 't/lib/';
	}
}

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
use_ok( 'Parse::CPAN::Meta' );
use_ok( 'Parse::CPAN::Meta::Test' );
