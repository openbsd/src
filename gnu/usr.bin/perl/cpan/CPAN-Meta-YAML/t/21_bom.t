#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use File::Spec::Functions ':ALL';
use t::lib::Test;
use Test::More tests(0, 1, 4);
use CPAN::Meta::YAML;





#####################################################################
# Testing that Perl::Smith config files work

my $sample_file = catfile( 't', 'data', 'utf_16_le_bom.yml' );
my $sample      = load_ok( 'utf_16_le_bom.yml', $sample_file, 3 );

# Does the string parse to the structure
my $name      = "utf-16";
my $yaml_copy = $sample;
my $yaml      = eval { CPAN::Meta::YAML->read_string( $yaml_copy ); };
is( $@, '', "$name: CPAN::Meta::YAML parses without error" );
is( $yaml_copy, $sample, "$name: CPAN::Meta::YAML does not modify the input string" );
SKIP: {
	skip( "Shortcutting after failure", 2 ) if $@;
	is( $yaml, undef, "file not parsed" );
	is( CPAN::Meta::YAML->errstr, "Stream has a non UTF-8 BOM", "correct error" );
}
