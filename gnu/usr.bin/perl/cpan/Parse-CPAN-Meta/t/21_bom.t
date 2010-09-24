#!/usr/bin/perl

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
use Parse::CPAN::Meta::Test;
use Test::More tests(0, 1, 3);





#####################################################################
# Testing that Perl::Smith config files work

my $sample_file = catfile( test_data_directory(), 'utf_16_le_bom.yml' );
my $sample      = load_ok( 'utf_16_le_bom.yml', $sample_file, 3 );

# Does the string parse to the structure
my $name      = "utf-16";
my $yaml_copy = $sample;
my $yaml      = eval { Parse::CPAN::Meta::Load( $yaml_copy ); };
is( $yaml_copy, $sample, "$name: Parse::CPAN::Meta::Load does not modify the input string" );
is( $yaml, undef, "file not parsed" );
ok( $@ =~ "Stream has a non UTF-8 Unicode Byte Order Mark", "correct error" );
