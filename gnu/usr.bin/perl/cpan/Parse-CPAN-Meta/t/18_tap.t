#!/usr/bin/perl

# Testing relating to functionality in the Test Anything Protocol

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
use Test::More tests(5, 0, 0);
use Parse::CPAN::Meta ();





#####################################################################
# TAP Tests

# Make sure we support x-foo keys
yaml_ok(
	"---\nx-foo: 1\n",
	[ { 'x-foo' => 1 } ],
	'x-foo key',
);

# Document ending (hash)
yaml_ok(
	  "---\n"
	. "  foo: bar\n"
	. "...\n",
	[ { foo => "bar" } ],
	'document_end_hash',
	noyamlpm   => 1,
	nosyck     => 1,
	noyamlperl => 1,
);

# Document ending (array)
yaml_ok(
	  "---\n"
	. "- foo\n"
	. "...\n",
	[ [ 'foo' ] ],
	'document_end_array',
	noyamlpm => 1,
	noyamlperl => 1,
);

# Multiple documents (simple)
yaml_ok(
	  "---\n"
	. "- foo\n"
	. "...\n"
	. "---\n"
	. "- foo\n"
	. "...\n",
	[ [ 'foo' ], [ 'foo' ] ],
	'multi_document_simple',
	noyamlpm   => 1,
	noyamlperl => 1,
);

# Multiple documents (whitespace-separated)
yaml_ok(
	  "---\n"
	. "- foo\n"
	. "...\n"
	. "\n"
	. "---\n"
	. "- foo\n"
	. "...\n",
	[ [ 'foo' ], [ 'foo' ] ],
	'multi_document_space',
	noyamlpm   => 1,
	noyamlperl => 1,
);
