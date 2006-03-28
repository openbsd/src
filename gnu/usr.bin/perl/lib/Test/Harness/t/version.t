#!/usr/bin/perl -Tw

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;

use Test::More tests => 3;

BEGIN {
    use_ok('Test::Harness');
}

my $ver = $ENV{HARNESS_VERSION} or die "HARNESS_VERSION not set";
like( $ver, qr/^2.\d\d(_\d\d)?$/, "Version is proper format" );
is( $ver, $Test::Harness::VERSION );
