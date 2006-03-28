#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib/');
    }
    else {
        unshift @INC, 't/lib/';
    }
}

use Test::More tests => 3;
use Config ();

BEGIN { use_ok 'ExtUtils::MakeMaker::Config'; }

is $Config{path_sep}, $Config::Config{path_sep};

eval {
    $Config{wibble} = 42;
};
is $Config{wibble}, 42;
