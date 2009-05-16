#!/usr/bin/perl -w
# $Id: plan_shouldnt_import.t,v 1.2 2009/05/16 21:42:57 simon Exp $

# plan() used to export functions by mistake [rt.cpan.org 8385]

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}


use Test::More ();
Test::More::plan(tests => 1);

Test::More::ok( !__PACKAGE__->can('ok'), 'plan should not export' );
