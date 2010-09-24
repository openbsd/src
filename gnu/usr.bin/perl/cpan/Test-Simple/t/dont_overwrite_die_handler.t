#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

# Make sure this is in place before Test::More is loaded.
my $handler_called;
BEGIN {
    $SIG{__DIE__} = sub { $handler_called++ };
}

use Test::More tests => 2;

ok !eval { die };
is $handler_called, 1, 'existing DIE handler not overridden';
