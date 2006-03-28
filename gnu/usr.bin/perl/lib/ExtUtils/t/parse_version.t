#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
    else {
        unshift @INC, 't/lib';
    }
}
chdir 't';

use Test::More tests => 10;
use ExtUtils::MakeMaker;

my %versions = ('$VERSION = 0.02'   => 0.02,
                '$VERSION = 0.0'    => 0.0,
                '$VERSION = -1.0'   => -1.0,
                '$VERSION = undef'  => 'undef',
                '$wibble  = 1.0'    => 'undef',
               );

while( my($code, $expect) = each %versions ) {
    open(FILE, ">VERSION.tmp") || die $!;
    print FILE "$code\n";
    close FILE;

    $_ = 'foo';
    is( MM->parse_version('VERSION.tmp'), $expect, $code );
    is( $_, 'foo', '$_ not leaked by parse_version' );

    unlink "VERSION.tmp";
}
