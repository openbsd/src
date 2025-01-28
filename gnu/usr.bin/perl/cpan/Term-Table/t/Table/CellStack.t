use Term::Table;
use Term::Table::Util;
use Test2::Tools::Tiny;

use utf8;
use strict;
use warnings;

use Term::Table::CellStack;

sub table { Term::Table->new(@_)->render }

my @table = table(
    max_width => 40,
    header    => ['a', 'b', 'c', 'd'],
    rows      => [
        [qw/aaaaaaaaaaaaaaaaaaaaaaaaaa bbbbbbbbbbbbbbbbbbbbb ccccccccccccccccccccccc ddddddddddddddddddddddddddddd/],
        [
            Term::Table::CellStack->new(cells => [
                Term::Table::Cell->new(border_left => '>', border_right => '<', value => 'aaa'),
                Term::Table::Cell->new(value => 'bbb'),
                Term::Table::Cell->new(border_left => '>', border_right => '<', value => 'ccc'),
            ]),
            Term::Table::CellStack->new(cells => [
                Term::Table::Cell->new(border_left => '>', border_right => '<', value => 'aaaaaaaaaaaaaaaaaaaaa'),
                Term::Table::Cell->new(value => 'bbbbbbbbbbbbbbbbbbbb'),
                Term::Table::Cell->new(border_left => '>', border_right => '<', value => 'ccccccccccccccccccccc'),
            ]),
        ],
        [qw/AAAAAAAAAAAAAAAAAAAAAAAAAA BBBBBBBBBBBBBBBBBBBBB CCCCCCCCCCCCCCCCCCCCCCC DDDDDDDDDDDDDDDDDDDDDDDDDDDDD/],
    ],
);

is_deeply(
    \@table,
    [
		'+-------+-------+-------+-------+',
		'| a     | b     | c     | d     |',
		'+-------+-------+-------+-------+',
		'| aaaaa | bbbbb | ccccc | ddddd |',
		'| aaaaa | bbbbb | ccccc | ddddd |',
		'| aaaaa | bbbbb | ccccc | ddddd |',
		'| aaaaa | bbbbb | ccccc | ddddd |',
		'| aaaaa | b     | ccc   | ddddd |',
		'| a     |       |       | dddd  |',
		'|       |       |       |       |',
		'|> aaa <|> aaa <|       |       |',
		'| bbb   |> aaa <|       |       |',
		'|> ccc <|> aaa <|       |       |',
		'|       |> aaa <|       |       |',
		'|       |> aaa <|       |       |',
		'|       |> aaa <|       |       |',
		'|       |> aaa <|       |       |',
		'|       | bbbbb |       |       |',
		'|       | bbbbb |       |       |',
		'|       | bbbbb |       |       |',
		'|       | bbbbb |       |       |',
		'|       |> ccc <|       |       |',
		'|       |> ccc <|       |       |',
		'|       |> ccc <|       |       |',
		'|       |> ccc <|       |       |',
		'|       |> ccc <|       |       |',
		'|       |> ccc <|       |       |',
		'|       |> ccc <|       |       |',
		'|       |       |       |       |',
		'| AAAAA | BBBBB | CCCCC | DDDDD |',
		'| AAAAA | BBBBB | CCCCC | DDDDD |',
		'| AAAAA | BBBBB | CCCCC | DDDDD |',
		'| AAAAA | BBBBB | CCCCC | DDDDD |',
		'| AAAAA | B     | CCC   | DDDDD |',
		'| A     |       |       | DDDD  |',
		'+-------+-------+-------+-------+',
    ],
    "Basic table, small width"
);

done_testing;
