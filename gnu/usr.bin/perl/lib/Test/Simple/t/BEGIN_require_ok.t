#!/usr/bin/perl -w
# $Id: BEGIN_require_ok.t,v 1.1 2009/05/16 21:42:57 simon Exp $

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use Test::More;

my $result;
BEGIN {
    eval {
        require_ok("Wibble");
    };
    $result = $@;
}

plan tests => 1;
like $result, '/^You tried to run a test without a plan/';
