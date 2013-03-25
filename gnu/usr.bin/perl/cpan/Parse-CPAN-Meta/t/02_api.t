#!/usr/bin/perl

# Testing of a known-bad file from an editor

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
use Parse::CPAN::Meta;
use Parse::CPAN::Meta::Test;
# use Test::More skip_all => 'Temporarily ignoring failing test';
use Test::More 'no_plan';

#####################################################################
# Testing that Perl::Smith config files work

my $want = {
  "abstract" => "a set of version requirements for a CPAN dist",
  "author"   => [ 'Ricardo Signes <rjbs@cpan.org>' ],
  "build_requires" => {
     "Test::More" => "0.88"
  },
  "configure_requires" => {
     "ExtUtils::MakeMaker" => "6.31"
  },
  "generated_by" => "Dist::Zilla version 2.100991",
  "license" => "perl",
  "meta-spec" => {
     "url" => "http://module-build.sourceforge.net/META-spec-v1.4.html",
     "version" => 1.4
  },
  "name" => "Version-Requirements",
  "recommends" => {},
  "requires" => {
     "Carp" => "0",
     "Scalar::Util" => "0",
     "version" => "0.77"
  },
  "resources" => {
     "repository" => "git://git.codesimply.com/Version-Requirements.git"
  },
  "version" => "0.101010",
};

my $meta_json = catfile( test_data_directory(), 'VR-META.json' );
my $meta_yaml = catfile( test_data_directory(), 'VR-META.yml' );

### YAML tests
{
  local $ENV{PERL_YAML_BACKEND}; # ensure we get CPAN::META::YAML

  is(Parse::CPAN::Meta->yaml_backend(), 'CPAN::Meta::YAML', 'yaml_backend()');
  my $from_yaml = Parse::CPAN::Meta->load_file( $meta_yaml );
  is_deeply($from_yaml, $want, "load from YAML file results in expected data");
}

{
  local $ENV{PERL_YAML_BACKEND}; # ensure we get CPAN::META::YAML

  my $yaml   = load_ok( 'VR-META.yml', $meta_yaml, 100);
  my $from_yaml = Parse::CPAN::Meta->load_yaml_string( $yaml );
  is_deeply($from_yaml, $want, "load from YAML str results in expected data");
}

SKIP: {
  skip "YAML module not installed", 2
    unless eval "require YAML; 1";
  local $ENV{PERL_YAML_BACKEND} = 'YAML';

  is(Parse::CPAN::Meta->yaml_backend(), 'YAML', 'yaml_backend()');
  my $yaml   = load_ok( 'VR-META.yml', $meta_yaml, 100);
  my $from_yaml = Parse::CPAN::Meta->load_yaml_string( $yaml );
  is_deeply($from_yaml, $want, "load_yaml_string using PERL_YAML_BACKEND");
}

### JSON tests
{
  # JSON tests with JSON::PP
  local $ENV{PERL_JSON_BACKEND}; # ensure we get JSON::PP

  is(Parse::CPAN::Meta->json_backend(), 'JSON::PP', 'json_backend()');
  my $from_json = Parse::CPAN::Meta->load_file( $meta_json );
  is_deeply($from_json, $want, "load from JSON file results in expected data");
}

{
  # JSON tests with JSON::PP
  local $ENV{PERL_JSON_BACKEND}; # ensure we get JSON::PP

  my $json   = load_ok( 'VR-META.json', $meta_json, 100);
  my $from_json = Parse::CPAN::Meta->load_json_string( $json );
  is_deeply($from_json, $want, "load from JSON str results in expected data");
}

{
  # JSON tests with JSON::PP, take 2
  local $ENV{PERL_JSON_BACKEND} = 0; # request JSON::PP

  my $json   = load_ok( 'VR-META.json', $meta_json, 100);
  my $from_json = Parse::CPAN::Meta->load_json_string( $json );
  is_deeply($from_json, $want, "load_json_string with PERL_JSON_BACKEND = 0");
}

{
  # JSON tests with JSON::PP, take 3
  local $ENV{PERL_JSON_BACKEND} = 'JSON::PP'; # request JSON::PP

  my $json   = load_ok( 'VR-META.json', $meta_json, 100);
  my $from_json = Parse::CPAN::Meta->load_json_string( $json );
  is_deeply($from_json, $want, "load_json_string with PERL_JSON_BACKEND = 'JSON::PP'");
}

SKIP: {
  skip "JSON module version 2.5 not installed", 2
    unless eval "require JSON; JSON->VERSION(2.5); 1";
  local $ENV{PERL_JSON_BACKEND} = 1;

  is(Parse::CPAN::Meta->json_backend(), 'JSON', 'json_backend()');
  my $json   = load_ok( 'VR-META.json', $meta_json, 100);
  my $from_json = Parse::CPAN::Meta->load_json_string( $json );
  is_deeply($from_json, $want, "load_json_string with PERL_JSON_BACKEND = 1");
}

