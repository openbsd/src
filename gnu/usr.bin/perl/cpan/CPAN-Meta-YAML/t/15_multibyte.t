#!/usr/bin/perl

# Testing of META.yml containing AVAR's name

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use File::Spec::Functions ':ALL';
use t::lib::Test;
use Test::More tests(0, 1, 5);
use CPAN::Meta::YAML;





#####################################################################
# Testing that Perl::Smith config files work

my $sample_file = catfile( test_data_directory(), 'multibyte.yml' );
my $sample      = load_ok( 'multibyte.yml', $sample_file, 450 );

# Does the string parse to the structure
my $name      = "multibyte";
my $yaml_copy = $sample;
my $yaml      = eval { CPAN::Meta::YAML->read_string( $yaml_copy ); };
is( $@, '', "$name: CPAN::Meta::YAML parses without error" );
is( $yaml_copy, $sample, "$name: CPAN::Meta::YAML does not modify the input string" );
SKIP: {
	skip( "Shortcutting after failure", 2 ) if $@;
	isa_ok( $yaml, 'CPAN::Meta::YAML' );
	is_deeply( $yaml->[0]->{build_requires}, {
		'Config'     => 0,
		'Test::More' => 0,
		'XSLoader'   => 0,
	}, 'build_requires ok' );
}

SKIP: {
	unless ( CPAN::Meta::YAML::HAVE_UTF8() ) {
		skip("no utf8 support", 1 );
	}
	eval { utf8::is_utf8('') };
	if ( $@ ) {
		skip("no is_utf8 to test with until 5.8.1", 1);
	}
	ok( utf8::is_utf8($yaml->[0]->{author}), "utf8 decoded" );
}
