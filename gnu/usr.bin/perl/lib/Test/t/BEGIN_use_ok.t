#!/usr/bin/perl -w

# [rt.cpan.org 28345]
#
# A use_ok() inside a BEGIN block lacking a plan would be silently ignored.

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
        use_ok("Wibble");
    };
    $result = $@;
}

plan tests => 1;
like $result, '/^You tried to run a test without a plan/';
