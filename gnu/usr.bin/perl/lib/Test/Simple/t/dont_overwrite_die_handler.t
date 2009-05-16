#!/usr/bin/perl -w
# $Id: dont_overwrite_die_handler.t,v 1.1 2009/05/16 21:42:57 simon Exp $

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
