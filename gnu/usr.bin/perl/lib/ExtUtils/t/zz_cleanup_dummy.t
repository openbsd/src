#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}
chdir 't';


use strict;
use Test::More tests => 2;
use File::Path;

rmtree('Big-Dummy');
ok(!-d 'Big-Dummy', 'Big-Dummy cleaned up');
rmtree('Problem-Module');
ok(!-d 'Problem-Module', 'Problem-Module cleaned up');
