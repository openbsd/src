#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

my $Exit_Code;
BEGIN {
    *CORE::GLOBAL::exit = sub { $Exit_Code = shift; };
}


use Test::Builder;
use Test::More;
use TieOut;

my $output = tie *FAKEOUT, 'TieOut';
my $TB = Test::More->builder;
$TB->output(\*FAKEOUT);

my $Test = Test::Builder->create;
$Test->level(0);

if( $] >= 5.005 ) {
    $Test->plan(tests => 2);
}
else {
    $Test->plan(skip_all => 
          'CORE::GLOBAL::exit, introduced in 5.005, is needed for testing');
}


plan tests => 4;

BAIL_OUT("ROCKS FALL! EVERYONE DIES!");


$Test->is_eq( $output->read, <<'OUT' );
1..4
Bail out!  ROCKS FALL! EVERYONE DIES!
OUT

$Test->is_eq( $Exit_Code, 255 );
