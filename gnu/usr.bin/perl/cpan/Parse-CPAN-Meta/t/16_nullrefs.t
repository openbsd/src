#!/usr/bin/perl

# Testing for null references

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
use Test::More tests(1);





#####################################################################
# Example Empty References

yaml_ok(
	<<'END_YAML',
--- []
--- {}
END_YAML
	[ [], {} ],
	'Empty references',
);
