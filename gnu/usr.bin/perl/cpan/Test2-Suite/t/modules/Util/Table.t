use Test2::Bundle::Extended;
BEGIN { $ENV{TABLE_TERM_SIZE} = 80 }
use Test2::Util::Table qw/table/;
use Test2::Util::Term qw/USE_GCS/;

imported_ok qw/table/;

subtest unicode_display_width => sub {
    my $wide = "foo bar baz 婧";

    my $have_gcstring = eval { require Unicode::GCString; 1 };

    subtest no_unicode_linebreak => sub {
        my @table = table('header' => [ 'a', 'b'], 'rows'   => [[ '婧', '߃' ]]);

        like(
            \@table,
            ["Unicode::GCString is not installed, table may not display all unicode characters properly"],
            "got unicode note"
        );
    } unless USE_GCS;

    subtest with_unicode_linebreak => sub {
        my @table = table(
            'header' => [ 'a', 'b'],
            'rows'   => [[ 'a婧b', '߃' ]],
            'max_width' => 80,
        );
        is(
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

subtest width => sub {
    my @table = table(
        max_width => 40,
        header => [ 'a', 'b', 'c', 'd' ],
        rows => [
            [ qw/aaaaaaaaaaaaaaaaaaaaaaaaaa bbbbbbbbbbbbbbbbbbbbb ccccccccccccccccccccccc ddddddddddddddddddddddddddddd/ ],
            [ qw/AAAAAAAAAAAAAAAAAAAAAAAAAA BBBBBBBBBBBBBBBBBBBBB CCCCCCCCCCCCCCCCCCCCCCC DDDDDDDDDDDDDDDDDDDDDDDDDDDDD/ ],
        ],
    );

    is(length($table[0]), validator('<=', '40', sub { my %p = @_; $p{got} <= $p{name} }), "width of table");

    is(
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

    is(length($table[0]), validator('<=', '60', sub { my %p = @_; $p{got} <= $p{name} }), "width of table");

    is(
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

    is(length($table[0]), validator('<=', '60', sub { my %p = @_; $p{got} <= $p{name} }), "width of table");

    is(
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

subtest collapse => sub {
    my @table = table(
        max_width => 60,
        collapse => 1,
        header => [ 'a', 'b', 'c', 'd' ],
        rows => [
            [ qw/aaaa bbbb/, undef, qw/dddd/ ],
            [ qw/AAAA BBBB/, '', qw/DDDD/ ],
        ],
    );

    is(
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
        header => [ 'a', 'b', 'c', 'd' ],
        rows => [
            [ qw/aaaa bbbb/, undef, qw/dddd/ ],
            [ qw/AAAA BBBB/, '', qw/DDDD/ ],
        ],
    );

    is(
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

    is(
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

subtest header => sub {
    my @table = table(
        max_width => 60,
        header => [ 'a', 'b', 'c', 'd' ],
        rows => [
            [ qw/aaaa bbbb cccc dddd/ ],
            [ qw/AAAA BBBB CCCC DDDD/ ],
        ],
    );

    is(
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

subtest no_header => sub {
    my @table = table(
        max_width => 60,
        rows => [
            [ qw/aaaa bbbb cccc dddd/ ],
            [ qw/AAAA BBBB CCCC DDDD/ ],
        ],
    );

    is(
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

subtest mark_tail => sub {
    my @table = table(
        max_width => 60,
        mark_tail => 1,
        header => [ 'data1', 'data2' ],
        rows => [["  abc  def   ", "  abc  def  \t"]],
    );

    is(
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
