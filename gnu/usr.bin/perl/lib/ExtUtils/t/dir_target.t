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
chdir 't';

use Test::More tests => 1;
use ExtUtils::MakeMaker;

# dir_target() was typo'd as dir_targets()
can_ok('MM', 'dir_target');
