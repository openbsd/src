#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}
chdir($^O eq 'VMS' ? 'BFD_TEST_ROOT:[t]' : 't');


use strict;
use Test::More tests => 3;
use File::Path;

rmtree('Big-Dummy');
ok(!-d 'Big-Dummy', 'Big-Dummy cleaned up');
rmtree('Problem-Module');
ok(!-d 'Problem-Module', 'Problem-Module cleaned up');
rmtree('dummy-install');
ok(!-d 'dummy-install', 'dummy-install cleaned up');
