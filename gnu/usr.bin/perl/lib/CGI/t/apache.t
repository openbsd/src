#!/usr/local/bin/perl -w

BEGIN {
	chdir 't' if -d 't';
	if ($ENV{PERL_CORE}) {
		@INC = '../lib';
	} else {
		# Due to a bug in older versions of MakeMaker & Test::Harness, we must
		# ensure the blib's are in @INC, else we might use the core CGI.pm
		unshift @INC, qw( ../blib/lib ../blib/arch lib );
	}
}

use strict;
use Test::More tests => 1;

# Can't do much with this other than make sure it loads properly
BEGIN { use_ok('CGI::Apache') };
