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

use Test::More tests => 4;
use Parse::CPAN::Meta;



ok not(defined &main::Load), 'Load is not exported';
ok not(defined &main::Dump), 'Dump is not exported';
ok not(defined &main::LoadFile), 'LoadFile is not exported';
ok not(defined &main::DumpFile), 'DumpFile is not exported';
