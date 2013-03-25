#!/usr/bin/perl

# Testing of basic document structures

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

use Test::More tests => 1;
use Parse::CPAN::Meta ();

my $one = <<'END_YAML';
---
- foo: - bar
END_YAML

my $one_scalar_tiny = eval { Parse::CPAN::Meta->load_yaml_string( $one ) };
like( $@, '/illegal characters/', "error causes exception");

