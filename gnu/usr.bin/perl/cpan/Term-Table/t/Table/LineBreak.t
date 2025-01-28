use Test2::Tools::Tiny;
use Term::Table::LineBreak;
use strict;
use warnings;
use utf8;

use Test2::API qw/test2_stack/;
test2_stack->top->format->encoding('utf8');

tests with_unicode_linebreak => sub {
    my $one = Term::Table::LineBreak->new(string => 'aaaa婧bbbb');
    $one->break(3);
    is_deeply(
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

    $one = Term::Table::LineBreak->new(string => 'a婧bb');
    $one->break(2);
    is_deeply(
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

tests without_unicode_linebreak => sub {
    my @parts;
    {
        local %INC = %INC;
        delete $INC{'Unicode/GCString.pm'};
        my $one = Term::Table::LineBreak->new(string => 'aaaa婧bbbb');
        $one->break(3);
        @parts = map { $one->next } 1 .. 5;
    }

    todo "Can't handle unicode properly without Unicode::GCString" => sub {
        is_deeply(
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

    my $one = Term::Table::LineBreak->new(string => 'aaabbbx');
    $one->break(2);
    is_deeply(
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
