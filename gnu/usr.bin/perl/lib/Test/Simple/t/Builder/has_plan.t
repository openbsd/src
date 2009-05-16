#!/usr/bin/perl -w
# $Id: has_plan.t,v 1.1 2009/05/16 21:42:57 simon Exp $

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib');
    }
}

use strict;
use Test::Builder;

my $unplanned;

BEGIN {
	$unplanned = 'oops';
	$unplanned = Test::Builder->new->has_plan;
};

use Test::More tests => 2;

is($unplanned, undef, 'no plan yet defined');
is(Test::Builder->new->has_plan, 2, 'has fixed plan');
