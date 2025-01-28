package Test2::Util::Table;
use strict;
use warnings;

our $VERSION = '0.000162';

use base 'Term::Table';

use Test2::Util::Importer 'Test2::Util::Importer' => 'import';
our @EXPORT_OK  = qw/table/;
our %EXPORT_GEN = (
    '&term_size' => sub {
        require Carp;
        Carp::cluck "term_size should be imported from Test2::Util::Term, not " . __PACKAGE__;
        Test2::Util::Term->can('term_size');
    },
);

sub table {
    my %params = @_;

    $params{collapse}    ||= 0;
    $params{sanitize}    ||= 0;
    $params{mark_tail}   ||= 0;
    $params{show_header} ||= 0 unless $params{header} && @{$params{header}};

    __PACKAGE__->new(%params)->render;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Util::Table - Format a header and rows into a table

=head1 DESCRIPTION

This is used by some failing tests to provide diagnostics about what has gone
wrong. This module is able to generic format rows of data into tables.

=head1 SYNOPSIS

    use Test2::Util::Table qw/table/;

    my @table = table(
        max_width => 80,
        collapse => 1, # Do not show empty columns
        header => [ 'name', 'age', 'hair color' ],
        rows => [
            [ 'Fred Flinstone',  2000000, 'black' ],
            [ 'Wilma Flinstone', 1999995, 'red' ],
            ...,
        ],
    );

    # The @table array contains each line of the table, no newlines added.
    say $_ for @table;

This prints a table like this:

    +-----------------+---------+------------+
    | name            | age     | hair color |
    +-----------------+---------+------------+
    | Fred Flinstone  | 2000000 | black      |
    | Wilma Flinstone | 1999995 | red        |
    | ...             | ...     | ...        |
    +-----------------+---------+------------+

=head1 EXPORTS

=head2 @rows = table(...)

The function returns a list of lines, lines do not have the newline C<\n>
character appended.

Options:

=over 4

=item header => [ ... ]

If you want a header specify it here. This takes an arrayref with each columns
heading.

=item rows => [ [...], [...], ... ]

This should be an arrayref containing an arrayref per row.

=item collapse => $bool

Use this if you want to hide empty columns, that is any column that has no data
in any row. Having a header for the column will not effect collapse.

=item max_width => $num

Set the maximum width of the table, the table may not be this big, but it will
be no bigger. If none is specified it will attempt to find the width of your
terminal and use that, otherwise it falls back to C<80>.

=item sanitize => $bool

This will sanitize all the data in the table such that newlines, control
characters, and all whitespace except for ASCII 20 C<' '> are replaced with
escape sequences. This prevents newlines, tabs, and similar whitespace from
disrupting the table.

B<Note:> newlines are marked as '\n', but a newline is also inserted into the
data so that it typically displays in a way that is useful to humans.

Example:

    my $field = "foo\nbar\nbaz\n";

    print join "\n" => table(
        sanitize => 1,
        rows => [
            [$field,      'col2'     ],
            ['row2 col1', 'row2 col2']
        ]
    );

Prints:

    +-----------------+-----------+
    | foo\n           | col2      |
    | bar\n           |           |
    | baz\n           |           |
    |                 |           |
    | row2 col1       | row2 col2 |
    +-----------------+-----------+

So it marks the newlines by inserting the escape sequence, but it also shows
the data across as many lines as it would normally display.

=item mark_tail => $bool

This will replace the last whitespace character of any trailing whitespace with
its escape sequence. This makes it easier to notice trailing whitespace when
comparing values.

=back

=head2 my $cols = term_size()

Attempts to find the width in columns (characters) of the current terminal.
Returns 80 as a safe bet if it cannot find it another way.

=head1 NOTE ON UNICODE/WIDE CHARACTERS

Some unicode characters, such as C<婧> (C<U+5A67>) are wider than others. These
will render just fine if you C<use utf8;> as necessary, and
L<Unicode::GCString> is installed, however if the module is not installed there
will be anomalies in the table:

    +-----+-----+---+
    | a   | b   | c |
    +-----+-----+---+
    | 婧 | x   | y |
    | x   | y   | z |
    | x   | 婧 | z |
    +-----+-----+---+

=head1 SOURCE

The source code repository for Test2-Suite can be found at
F<https://github.com/Test-More/Test2-Suite/>.

=head1 MAINTAINERS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 AUTHORS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright 2018 Chad Granum E<lt>exodist@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://dev.perl.org/licenses/>

=cut
