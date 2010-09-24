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
use Parse::CPAN::Meta::Test;
# use Test::More skip_all => 'Temporarily ignoring failing test';
use Test::More tests(1, 1);





#####################################################################
# Testing that Perl::Smith config files work

my $toolbar_file = catfile( test_data_directory(), 'toolbar.yml' );
my $toolbar      = load_ok( 'toolbar.yml', $toolbar_file, 100 );

yaml_ok(
	$toolbar,
	[ {
		main_toolbar => [
			'item file-new',
			'item file-open',
			'item file-print#',
			'item file-close#',
			'item file-save-all',
			'item file-save',
			undef,
			'item edit-changes-undo',
			'item edit-changes-redo',
			undef,
			'item edit-cut',
			'item edit-copy',
			'item edit-paste',
			'item edit-replace',
			'item edit-delete',
		]
	} ],
	'toolbar.yml',
	noyamlperl => 1,
);
