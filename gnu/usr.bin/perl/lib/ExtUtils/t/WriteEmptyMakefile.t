#!/usr/bin/perl -w

# This is a test of WriteEmptyMakefile.

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

chdir 't';

use strict;
use Test::More tests => 5;

use ExtUtils::MakeMaker qw(WriteEmptyMakefile);
use TieOut;

can_ok __PACKAGE__, 'WriteEmptyMakefile';

eval { WriteEmptyMakefile("something"); };
like $@, qr/Need an even number of args/;


{
    ok( my $stdout = tie *STDOUT, 'TieOut' );

    ok !-e 'wibble';
    END { 1 while unlink 'wibble' }

    WriteEmptyMakefile(
        NAME            => "Foo",
        FIRST_MAKEFILE  => "wibble",
    );
    ok -e 'wibble';
}
