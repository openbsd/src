#!/usr/bin/perl -w

BEGIN {
   if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = qw(../lib ../lib/base/t/lib);
    }
}

use strict;

use Test::More tests => 1;

# Here we emulate a bug with base.pm not finding the Exporter version
# for some reason.
use lib qw(t/lib);
use base qw(Dummy);

is( $Dummy::VERSION, 5.562,       "base.pm doesn't confuse the version" );
