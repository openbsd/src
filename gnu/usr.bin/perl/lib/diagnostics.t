#!./perl

BEGIN {
    if ($^O eq 'MacOS') {
	chdir '::' if -d '::pod' && -d '::t';
	@INC = ':lib:';
    } else {
	chdir '..' if -d '../pod' && -d '../t';
	@INC = 'lib';
    }
}

use Test::More tests => 2;

BEGIN { use_ok('diagnostics') }

require base;

eval {
    'base'->import(qw(I::do::not::exist));
};

like( $@, qr/^Base class package "I::do::not::exist" is empty/);
