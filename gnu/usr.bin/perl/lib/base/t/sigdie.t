#!/usr/bin/perl -w

BEGIN {
   if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = qw(../lib ../lib/base/t/lib);
    }
}

use strict;
use Test::More tests => 2;

use base;

{
    package Test::SIGDIE;

    local $SIG{__DIE__} = sub { 
        ::fail('sigdie not caught, this test should not run') 
    };
    eval {
      'base'->import(qw(Huh::Boo));
    };

    ::like($@, qr/^Base class package "Huh::Boo" is empty/, 
         'Base class empty error message');
}


{
    use lib 't/lib';
    
    local $SIG{__DIE__};
    base->import(qw(HasSigDie));
    ok $SIG{__DIE__}, 'base.pm does not mask SIGDIE';
}
