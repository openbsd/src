#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

use Test::More;
use Config;

if( !$Config{d_fork} ) {
    plan skip_all => "This system cannot fork";
}
else {
    plan tests => 1;
}

if( fork ) { # parent
    pass("Only the parent should process the ending, not the child");
}
else {
    exit;   # child
}
