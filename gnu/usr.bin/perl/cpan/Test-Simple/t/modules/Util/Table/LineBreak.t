use Test2::Bundle::Extended;
use Test2::Util::Table::LineBreak;

BEGIN { $ENV{TABLE_TERM_SIZE} = 80 }

subtest with_unicode_linebreak => sub {
    my $one = Test2::Util::Table::LineBreak->new(string => 'aaaa婧bbbb');
    $one->break(3);
    is(
        [ map { $one->next } 1 .. 5 ],
        [
            'aaa',
            'a婧',
            'bbb',
            'b  ',
            undef
        ],
        "Got all parts"
    );

    $one = Test2::Util::Table::LineBreak->new(string => 'a婧bb');
    $one->break(2);
    is(
        [ map { $one->next } 1 .. 4 ],
        [
            'a ',
            '婧',
            'bb',
            undef
        ],
        "Padded the problem"
    );

} if $INC{'Unicode/LineBreak.pm'};

subtest without_unicode_linebreak => sub {
    my @parts;
    {
        local %INC = %INC;
        delete $INC{'Unicode/GCString.pm'};
        my $one = Test2::Util::Table::LineBreak->new(string => 'aaaa婧bbbb');
        $one->break(3);
        @parts = map { $one->next } 1 .. 5;
    }

    todo "Can't handle unicode properly without Unicode::GCString" => sub {
        is(
            \@parts,
            [
                'aaa',
                'a婧',
                'bbb',
                'b  ',
                undef
            ],
            "Got all parts"
        );
    };

    my $one = Test2::Util::Table::LineBreak->new(string => 'aaabbbx');
    $one->break(2);
    is(
        [ map { $one->next } 1 .. 5 ],
        [
            'aa',
            'ab',
            'bb',
            'x ',
            undef
        ],
        "Padded the problem"
    );
};

done_testing;
