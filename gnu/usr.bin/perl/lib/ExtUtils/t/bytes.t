#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use Test::More tests => 4;

use_ok('ExtUtils::MakeMaker::bytes');

SKIP: {
    skip "bytes.pm appeared in 5.6", 3 if $] < 5.006;

    my $chr = chr(400);
    is( length $chr, 1 );

    {
        use ExtUtils::MakeMaker::bytes;
        is( length $chr, 2, 'byte.pm in effect' );
    }

    is( length $chr, 1, '  score is lexical' );
}
