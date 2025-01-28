use Term::Table;
use Term::Table::Util qw/USE_GCS/;

use Test2::Tools::Tiny;

use utf8;
use strict;
use warnings;

use Test2::API qw/test2_stack/;
test2_stack->top->format->encoding('utf8');

sub table { Term::Table->new(@_)->render }

tests unicode_display_width => sub {
    my $wide = "foo bar baz 婧";

    my $have_gcstring = eval { require Unicode::GCString; 1 };

    tests no_unicode_linebreak => sub {
        my @table = table('header' => [ 'a', 'b'], 'rows'   => [[ '婧', '߃' ]]);

        is(
            $table[0],
            "Unicode::GCString is not installed, table may not display all unicode characters properly",
            "got unicode note"
        );
    } unless USE_GCS;

    tests with_unicode_linebreak => sub {
        my @table = table(
            'header' => [ 'a', 'b'],
            'rows'   => [[ 'a婧b', '߃' ]],
            'max_width' => 80,
        );
        is_deeply(
            \@table,
            [
                '+------+---+',
                '| a    | b |',
                '+------+---+',
                '| a婧b | ߃ |',
                '+------+---+',
            ],
            "Support for unicode characters that use multiple columns"
        );
    } if USE_GCS;
};

tests width => sub {
    my @table = table(
        max_width => 40,
        header => [ 'a', 'b', 'c', 'd' ],
        rows => [
            [ qw/aaaaaaaaaaaaaaaaaaaaaaaaaa bbbbbbbbbbbbbbbbbbbbb ccccccccccccccccccccccc ddddddddddddddddddddddddddddd/ ],
            [ qw/AAAAAAAAAAAAAAAAAAAAAAAAAA BBBBBBBBBBBBBBBBBBBBB CCCCCCCCCCCCCCCCCCCCCCC DDDDDDDDDDDDDDDDDDDDDDDDDDDDD/ ],
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

    @table = table(
        max_width => 60,
        header => [ 'a', 'b', 'c', 'd' ],
        rows => [
            [ qw/aaaaaaaaaaaaaaaaaaaaaaaaaa bbbbbbbbbbbbbbbbbbbbb ccccccccccccccccccccccc ddddddddddddddddddddddddddddd/ ],
            [ qw/AAAAAAAAAAAAAAAAAAAAAAAAAA BBBBBBBBBBBBBBBBBBBBB CCCCCCCCCCCCCCCCCCCCCCC DDDDDDDDDDDDDDDDDDDDDDDDDDDDD/ ],
        ],
    );

    is_deeply(
        \@table,
        [
            '+------------+------------+------------+------------+',
            '| a          | b          | c          | d          |',
            '+------------+------------+------------+------------+',
            '| aaaaaaaaaa | bbbbbbbbbb | cccccccccc | dddddddddd |',
            '| aaaaaaaaaa | bbbbbbbbbb | cccccccccc | dddddddddd |',
            '| aaaaaa     | b          | ccc        | ddddddddd  |',
            '|            |            |            |            |',
            '| AAAAAAAAAA | BBBBBBBBBB | CCCCCCCCCC | DDDDDDDDDD |',
            '| AAAAAAAAAA | BBBBBBBBBB | CCCCCCCCCC | DDDDDDDDDD |',
            '| AAAAAA     | B          | CCC        | DDDDDDDDD  |',
            '+------------+------------+------------+------------+',
        ],
        "Basic table, bigger width"
    );

    @table = table(
        max_width => 60,
        header => [ 'a', 'b', 'c', 'd' ],
        rows => [
            [ qw/aaaa bbbb cccc dddd/ ],
            [ qw/AAAA BBBB CCCC DDDD/ ],
        ],
    );

    is_deeply(
        \@table,
        [
            '+------+------+------+------+',
            '| a    | b    | c    | d    |',
            '+------+------+------+------+',
            '| aaaa | bbbb | cccc | dddd |',
            '| AAAA | BBBB | CCCC | DDDD |',
            '+------+------+------+------+',
        ],
        "Short table, well under minimum",
    );
};

tests collapse => sub {
    my @table = table(
        max_width => 60,
        collapse => 1,
        header => [ 'a', 'b', 'c', 'd' ],
        rows => [
            [ qw/aaaa bbbb/, undef, qw/dddd/ ],
            [ qw/AAAA BBBB/, '', qw/DDDD/ ],
        ],
    );

    is_deeply(
        \@table,
        [
            '+------+------+------+',
            '| a    | b    | d    |',
            '+------+------+------+',
            '| aaaa | bbbb | dddd |',
            '| AAAA | BBBB | DDDD |',
            '+------+------+------+',
        ],
        "Table collapsed",
    );

    @table = table(
        max_width => 60,
        collapse => 0,
        header => [ 'a', 'b', 'c', 'd' ],
        rows => [
            [ qw/aaaa bbbb/, undef, qw/dddd/ ],
            [ qw/AAAA BBBB/, '', qw/DDDD/ ],
        ],
    );

    is_deeply(
        \@table,
        [
            '+------+------+---+------+',
            '| a    | b    | c | d    |',
            '+------+------+---+------+',
            '| aaaa | bbbb |   | dddd |',
            '| AAAA | BBBB |   | DDDD |',
            '+------+------+---+------+',
        ],
        "Table not collapsed",
    );

    @table = table(
        max_width => 60,
        collapse => 1,
        header => [ 'a', 'b', 'c', 'd' ],
        rows => [
            [ qw/aaaa bbbb/, undef, qw/dddd/ ],
            [ qw/AAAA BBBB/, 0, qw/DDDD/ ],
        ],
    );

    is_deeply(
        \@table,
        [
            '+------+------+---+------+',
            '| a    | b    | c | d    |',
            '+------+------+---+------+',
            '| aaaa | bbbb |   | dddd |',
            '| AAAA | BBBB | 0 | DDDD |',
            '+------+------+---+------+',
        ],
        "'0' value does not cause collapse",
    );

};

tests header => sub {
    my @table = table(
        max_width => 60,
        header => [ 'a', 'b', 'c', 'd' ],
        rows => [
            [ qw/aaaa bbbb cccc dddd/ ],
            [ qw/AAAA BBBB CCCC DDDD/ ],
        ],
    );

    is_deeply(
        \@table,
        [
            '+------+------+------+------+',
            '| a    | b    | c    | d    |',
            '+------+------+------+------+',
            '| aaaa | bbbb | cccc | dddd |',
            '| AAAA | BBBB | CCCC | DDDD |',
            '+------+------+------+------+',
        ],
        "Table with header",
    );
};

tests no_header => sub {
    my @table = table(
        max_width => 60,
        rows => [
            [ qw/aaaa bbbb cccc dddd/ ],
            [ qw/AAAA BBBB CCCC DDDD/ ],
        ],
    );

    is_deeply(
        \@table,
        [
            '+------+------+------+------+',
            '| aaaa | bbbb | cccc | dddd |',
            '| AAAA | BBBB | CCCC | DDDD |',
            '+------+------+------+------+',
        ],
        "Table without header",
    );
};

tests sanitize => sub {
    my @table = table(
        max_width => 60,
        sanitize => 1,
        header => [ 'data1' ],
        rows => [["a\t\n\r\b\a          　‌﻿\N{U+000B}bф"]],
    );

    my $have_gcstring = eval { require Unicode::GCString; 1 } || 0;

    is_deeply(
        \@table,
        [
            (
                $have_gcstring
                ? ()
                : ("Unicode::GCString is not installed, table may not display all unicode characters properly")
            ),
            '+------------------------------------------------------+',
            '| data1                                                |',
            '+------------------------------------------------------+',
            '| a\\t\\n                                                |',
            '| \\r\\b\\a\\N{U+A0}\\N{U+1680}\\N{U+2000}\\N{U+2001}\\N{U+200 |',
            '| 2}\\N{U+2003}\\N{U+2004}\\N{U+2008}\\N{U+2028}\\N{U+2029} |',
            "| \\N{U+3000}\\N{U+200C}\\N{U+FEFF}\\N{U+B}b\x{444}              |",
            '+------------------------------------------------------+'
        ],
        "Sanitized data"
    );
};

tests mark_tail => sub {
    my @table = table(
        max_width => 60,
        mark_tail => 1,
        header => [ 'data1', 'data2' ],
        rows => [["  abc  def   ", "  abc  def  \t"]],
    );

    is_deeply(
        \@table,
        [
            '+----------------------+----------------+',
            '| data1                | data2          |',
            '+----------------------+----------------+',
            '|   abc  def  \N{U+20} |   abc  def  \t |',
            '+----------------------+----------------+',
        ],
        "Sanitized data"
    );

};

done_testing;
