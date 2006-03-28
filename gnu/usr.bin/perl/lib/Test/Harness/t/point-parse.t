#!/usr/bin/perl -w

BEGIN {
    if ( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;

use Test::More tests => 52;

BEGIN {
    use_ok( 'Test::Harness::Point' );
    use_ok( 'Test::Harness::Straps' );
}

my $strap = Test::Harness::Straps->new;
isa_ok( $strap, 'Test::Harness::Straps', 'new()' );


my $testlines = {
    'not ok' => {
        ok => 0
    },
    'not ok # TODO' => {
        ok => 0,
        reason => '',
        type => 'todo'
    },
    'not ok 1' => {
        number => 1,
        ok => 0
    },
    'not ok 11 - this is \\# all the name # skip this is not' => {
        description => 'this is \\# all the name',
        number => 11,
        ok => 0,
        reason => 'this is not',
        type => 'skip'
    },
    'not ok 23 # TODO world peace' => {
        number => 23,
        ok => 0,
        reason => 'world peace',
        type => 'todo'
    },
    'not ok 42 - universal constant' => {
        description => 'universal constant',
        number => 42,
        ok => 0
    },
    ok => {
        ok => 1
    },
    'ok # skip' => {
        ok => 1,
        type => 'skip'
    },
    'ok 1' => {
        number => 1,
        ok => 1
    },
    'ok 1066 - and all that' => {
        description => 'and all that',
        number => 1066,
        ok => 1
    },
    'ok 11 - have life # TODO get a life' => {
        description => 'have life',
        number => 11,
        ok => 1,
        reason => 'get a life',
        type => 'todo'
    },
    'ok 2938' => {
        number => 2938,
        ok => 1
    },
    'ok 42 - _is_header() is a header \'1..192 todo 4 2 13 192 \\# Skip skip skip because' => {
        description => '_is_header() is a header \'1..192 todo 4 2 13 192 \\# Skip skip skip because',
        number => 42,
        ok => 1
    }
};
my @untests = (
               ' ok',
               'not',
               'okay 23',
              );

for my $line ( sort keys %$testlines ) {
    my $point = Test::Harness::Point->from_test_line( $line );
    isa_ok( $point, 'Test::Harness::Point' );

    my $fields = $testlines->{$line};
    for my $property ( sort keys %$fields ) {
        my $value = $fields->{$property};
        is( eval "\$point->$property", $value, "$property on $line" );
        # Perls pre-5.6 can't handle $point->$property, and must be eval()d
    }
}
