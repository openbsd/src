#!/usr/bin/perl -w
# $Id: tbm_doesnt_set_exported_to.t,v 1.1 2009/05/16 21:42:57 simon Exp $

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

use strict;
use warnings;

# Can't use Test::More, that would set exported_to()
use Test::Builder;
use Test::Builder::Module;

my $TB = Test::Builder->create;
$TB->plan( tests => 1 );
$TB->level(0);

$TB->is_eq( Test::Builder::Module->builder->exported_to,
            undef,
            'using Test::Builder::Module does not set exported_to()'
);
