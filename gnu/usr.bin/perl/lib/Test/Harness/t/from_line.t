#!perl -Tw

BEGIN {
    if ( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use Test::More tests => 23;

BEGIN {
    use_ok( 'Test::Harness::Point' );
}

BASIC_OK: {
    my $line = "ok 14 - Blah blah";
    my $point = Test::Harness::Point->from_test_line( $line );
    isa_ok( $point, 'Test::Harness::Point', 'BASIC_OK' );
    is( $point->number, 14 );
    ok( $point->ok );
    is( $point->description, 'Blah blah' );
}

BASIC_NOT_OK: {
    my $line = "not ok 267   Yada";
    my $point = Test::Harness::Point->from_test_line( $line );
    isa_ok( $point, 'Test::Harness::Point', 'BASIC_NOT_OK' );
    is( $point->number, 267 );
    ok( !$point->ok );
    is( $point->description, 'Yada' );
}

CRAP: {
    my $point = Test::Harness::Point->from_test_line( 'ok14 - Blah' );
    ok( !defined $point, 'CRAP 1' );

    $point = Test::Harness::Point->from_test_line( 'notok 14' );
    ok( !defined $point, 'CRAP 2' );
}

PARSE_TODO: {
    my $point = Test::Harness::Point->from_test_line( 'not ok 14 - Calculate sqrt(-1) # TODO Still too rational' );
    isa_ok( $point, 'Test::Harness::Point', 'PARSE_TODO' );
    is( $point->description, 'Calculate sqrt(-1)' );
    is( $point->directive_type, 'todo' );
    is( $point->directive_reason, 'Still too rational' );
    ok( !$point->is_skip );
    ok( $point->is_todo );
}

PARSE_SKIP: {
    my $point = Test::Harness::Point->from_test_line( 'ok 14 # skip Not on bucket #6' );
    isa_ok( $point, 'Test::Harness::Point', 'PARSE_SKIP' );
    is( $point->description, '' );
    is( $point->directive_type, 'skip' );
    is( $point->directive_reason, 'Not on bucket #6' );
    ok( $point->is_skip );
    ok( !$point->is_todo );
}
