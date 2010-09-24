#!/usr/bin/perl

# Testing of common META.yml examples

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
use Test::More tests(1, 1);





#####################################################################
# Testing that Perl::Smith config files work

my $sample_file = catfile( test_data_directory(), 'sample.yml' );
my $sample      = load_ok( 'sample.yml', $sample_file, 500 );

yaml_ok(
	$sample,
	[ {
		invoice   => 34843,
		date      => '2001-01-23',
		'bill-to' => {
			given  => 'Chris',	
			family => 'Dumars',
			address => {
				lines  => "458 Walkman Dr.\nSuite #292\n",
				city   => 'Royal Oak',
				state  => 'MI',
				postal => 48046,
			},
		},
		product => [
			{
				sku         => 'BL394D',
				quantity    => '4',
				description => 'Basketball',
				price       => '450.00',
			},
			{
				sku         => 'BL4438H',
				quantity    => '1',
				description => 'Super Hoop',
				price       => '2392.00',
			},
		],
		tax      => '251.42',
		total    => '4443.52',
		comments => <<'END_TEXT',
Late afternoon is best. Backup contact is Nancy Billsmer @ 338-4338.
END_TEXT
	} ],
	'sample.yml',
	# nosyck => 1,
);
