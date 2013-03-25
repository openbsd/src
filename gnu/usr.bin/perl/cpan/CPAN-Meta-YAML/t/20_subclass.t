#!/usr/bin/perl

# Testing documents that should fail

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use File::Spec::Functions ':ALL';
use t::lib::Test;
use Test::More tests => 1;





#####################################################################
# Customized Class

SCOPE: {
	package Foo;

	use CPAN::Meta::YAML;

	use vars qw{@ISA};
	BEGIN {
		@ISA = 'CPAN::Meta::YAML';
	}

	sub _write_scalar {
		my $self   = shift;
		my $string = shift;
		my $indent = shift;
		if ( defined $indent ) {
			return "'$indent'";
		} else {
			return 'undef';
		}
	}

	1;
}





#####################################################################
# Generate the value

my $object = Foo->new(
	{ foo => 'bar' }
);
is( $object->write_string, "---\nfoo: '1'\n", 'Subclassing works' );
