#!./perl 

BEGIN {
    chdir 't' if -d 't';
    push @INC, '../lib';
}

use Test::More tests => 1;

use_ok( 'less' );
