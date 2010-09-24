#!/usr/bin/perl

# Testing documents that should fail

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
use Test::More tests => 1;





#####################################################################
# Missing Features

# We don't support raw nodes
yaml_error( <<'END_YAML', 'does not support a feature' );
---
version: !!perl/hash:version 
  original: v2.0.2
  qv: 1
  version: 
    - 2
    - 0
    - 2
END_YAML

