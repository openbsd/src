#!/usr/bin/perl -w

# Regression test some quirky behavior of base.pm.

BEGIN {
   if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = qw(../lib);
    }
}

use strict;
use Test::More tests => 1;

{
    package Parent;

    sub foo { 42 }

    package Middle;

    use base qw(Parent);

    package Child;

    base->import(qw(Middle Parent));
}

is_deeply [@Child::ISA], [qw(Middle)],
          'base.pm will not add to @ISA if you already are-a';