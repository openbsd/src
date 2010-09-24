#!/usr/bin/perl -w

use strict;
use lib 't/lib';

use TAP::Parser::Utils qw( split_shell );
use Test::More;

my @schedule = (
    {   name => 'Bare words',
        in   => 'bare words are here',
        out  => [ 'bare', 'words', 'are', 'here' ],
    },
    {   name => 'Single quotes',
        in   => "'bare' 'words' 'are' 'here'",
        out  => [ 'bare', 'words', 'are', 'here' ],
    },
    {   name => 'Double quotes',
        in   => '"bare" "words" "are" "here"',
        out  => [ 'bare', 'words', 'are', 'here' ],
    },
    {   name => 'Escapes',
        in   => '\  "ba\"re" \'wo\\\'rds\' \\\\"are" "here"',
        out  => [ ' ', 'ba"re', "wo'rds", '\\are', 'here' ],
    },
    {   name => 'Flag',
        in   => '-e "system(shift)"',
        out  => [ '-e', 'system(shift)' ],
    },
    {   name => 'Nada',
        in   => undef,
        out  => [],
    },
    {   name => 'Nada II',
        in   => '',
        out  => [],
    },
    {   name => 'Zero',
        in   => 0,
        out  => ['0'],
    },
    {   name => 'Empty',
        in   => '""',
        out  => [''],
    },
    {   name => 'Empty II',
        in   => "''",
        out  => [''],
    },
);

plan tests => 1 * @schedule;

for my $test (@schedule) {
    my $name = $test->{name};
    my @got  = split_shell( $test->{in} );
    unless ( is_deeply \@got, $test->{out}, "$name: parse OK" ) {
        use Data::Dumper;
        diag( Dumper( { want => $test->{out}, got => \@got } ) );
    }
}
