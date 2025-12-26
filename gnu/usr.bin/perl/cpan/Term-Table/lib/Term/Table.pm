package Term::Table;
use strict;
use warnings;

our $VERSION = '0.024';

use Term::Table::Cell();

use Term::Table::Util qw/term_size uni_length USE_GCS/;
use Scalar::Util qw/blessed/;
use List::Util qw/max sum/;
use Carp qw/croak carp/;

use Term::Table::HashBase qw/rows _columns collapse max_width mark_tail sanitize show_header auto_columns no_collapse header allow_overflow pad/;

sub BORDER_SIZE()   { 4 }    # '| ' and ' |' borders
sub DIV_SIZE()      { 3 }    # ' | ' column delimiter
sub CELL_PAD_SIZE() { 2 }    # space on either side of the |

sub init {
    my $self = shift;

    croak "You cannot have a table with no rows"
        unless $self->{+ROWS} && @{$self->{+ROWS}};

    $self->{+MAX_WIDTH}   ||= term_size();
    $self->{+NO_COLLAPSE} ||= {};
    if (ref($self->{+NO_COLLAPSE}) eq 'ARRAY') {
        $self->{+NO_COLLAPSE} = {map { ($_ => 1) } @{$self->{+NO_COLLAPSE}}};
    }

    if ($self->{+NO_COLLAPSE} && $self->{+HEADER}) {
        my $header = $self->{+HEADER};
        for(my $idx = 0; $idx < @$header; $idx++) {
            $self->{+NO_COLLAPSE}->{$idx} ||= $self->{+NO_COLLAPSE}->{$header->[$idx]};
        }
    }

    $self->{+PAD} = 4 unless defined $self->{+PAD};

    $self->{+COLLAPSE}  = 1 unless defined $self->{+COLLAPSE};
    $self->{+SANITIZE}  = 1 unless defined $self->{+SANITIZE};
    $self->{+MARK_TAIL} = 1 unless defined $self->{+MARK_TAIL};

    if($self->{+HEADER}) {
        $self->{+SHOW_HEADER}  = 1 unless defined $self->{+SHOW_HEADER};
    }
    else {
        $self->{+HEADER}       = [];
        $self->{+AUTO_COLUMNS} = 1;
        $self->{+SHOW_HEADER}  = 0;
    }
}

sub columns {
    my $self = shift;

    $self->regen_columns unless $self->{+_COLUMNS};

    return $self->{+_COLUMNS};
}

sub regen_columns {
    my $self = shift;

    my $has_header = $self->{+SHOW_HEADER} && @{$self->{+HEADER}};
    my %new_col = (width => 0, count => $has_header ? -1 : 0);

    my $cols = [map { {%new_col} } @{$self->{+HEADER}}];
    my @rows = @{$self->{+ROWS}};

    for my $row ($has_header ? ($self->{+HEADER}, @rows) : (@rows)) {
        for my $ci (0 .. max(@$cols - 1, @$row - 1)) {
            $cols->[$ci] ||= {%new_col} if $self->{+AUTO_COLUMNS};
            my $c = $cols->[$ci] or next;
            $c->{idx}  ||= $ci;
            $c->{rows} ||= [];

            my $r = $row->[$ci];
            $r = Term::Table::Cell->new(value => $r)
                unless blessed($r)
                && ($r->isa('Term::Table::Cell')
                || $r->isa('Term::Table::CellStack')
                || $r->isa('Term::Table::Spacer'));

            $r->sanitize  if $self->{+SANITIZE};
            $r->mark_tail if $self->{+MARK_TAIL};

            my $rs = $r->width;
            $c->{width} = $rs if $rs > $c->{width};
            $c->{count}++ if $rs;

            push @{$c->{rows}} => $r;
        }
    }

    # Remove any empty columns we can
    @$cols = grep {$_->{count} > 0 || $self->{+NO_COLLAPSE}->{$_->{idx}}} @$cols
        if $self->{+COLLAPSE};

    my $current = sum(map {$_->{width}} @$cols);
    my $border = sum(BORDER_SIZE, $self->{+PAD}, DIV_SIZE * (@$cols - 1));
    my $total = $current + $border;

    if ($total > $self->{+MAX_WIDTH}) {
        my $fair = ($self->{+MAX_WIDTH} - $border) / @$cols;
        if ($fair < 1) {
            return $self->{+_COLUMNS} = $cols if $self->{+ALLOW_OVERFLOW};
            croak "Table is too large ($total including $self->{+PAD} padding) to fit into max-width ($self->{+MAX_WIDTH})";
        }

        my $under = 0;
        my @fix;
        for my $c (@$cols) {
            if ($c->{width} > $fair) {
                push @fix => $c;
            }
            else {
                $under += $c->{width};
            }
        }

        # Recalculate fairness
        $fair = int(($self->{+MAX_WIDTH} - $border - $under) / @fix);
        if ($fair < 1) {
            return $self->{+_COLUMNS} = $cols if $self->{+ALLOW_OVERFLOW};
            croak "Table is too large ($total including $self->{+PAD} padding) to fit into max-width ($self->{+MAX_WIDTH})";
        }

        # Adjust over-long columns
        $_->{width} = $fair for @fix;
    }

    $self->{+_COLUMNS} = $cols;
}

sub render {
    my $self = shift;

    my $cols = $self->columns;
    for my $col (@$cols) {
        for my $cell (@{$col->{rows}}) {
            $cell->reset;
        }
    }
    my $width = sum(BORDER_SIZE, $self->{+PAD}, DIV_SIZE * @$cols, map { $_->{width} } @$cols);

    #<<< NO-TIDY
    my $border   = '+' . join('+', map { '-' x ($_->{width}  + CELL_PAD_SIZE) }      @$cols) . '+';
    my $template = '|' . join('|', map { my $w = $_->{width} + CELL_PAD_SIZE; '%s' } @$cols) . '|';
    my $spacer   = '|' . join('|', map { ' ' x ($_->{width}  + CELL_PAD_SIZE) }      @$cols) . '|';
    #>>>

    my @out = ($border);
    my ($row, $split, $found) = (0, 0, 0);
    while(1) {
        my @row;

        my $is_spacer = 0;

        for my $col (@$cols) {
            my $r = $col->{rows}->[$row];
            unless($r) {
                push @row => '';
                next;
            }

            my ($v, $vw);

            if ($r->isa('Term::Table::Cell')) {
                my $lw = $r->border_left_width;
                my $rw = $r->border_right_width;
                $vw = $col->{width} - $lw - $rw;
                $v = $r->break->next($vw);
            }
            elsif ($r->isa('Term::Table::CellStack')) {
                ($v, $vw) = $r->break->next($col->{width});
            }
            elsif ($r->isa('Term::Table::Spacer')) {
                $is_spacer = 1;
            }

            if ($is_spacer) {
                last;
            }
            elsif (defined $v) {
                $found++;
                my $bcolor = $r->border_color || '';
                my $vcolor = $r->value_color  || '';
                my $reset  = $r->reset_color  || '';

                if (my $need = $vw - uni_length($v)) {
                    $v .= ' ' x $need;
                }

                my $rt = "${reset}${bcolor}\%s${reset} ${vcolor}\%s${reset} ${bcolor}\%s${reset}";
                push @row => sprintf($rt, $r->border_left || '', $v, $r->border_right || '');
            }
            else {
                push @row => ' ' x ($col->{width} + 2);
            }
        }

        if (!grep {$_ && m/\S/} @row) {
            last unless $found || $is_spacer;

            push @out => $border if $row == 0 && $self->{+SHOW_HEADER} && @{$self->{+HEADER}};
            push @out => $spacer if $split > 1 || $is_spacer;

            $row++;
            $split = 0;
            $found = 0;

            next;
        }

        if ($split == 1 && @out > 1 && $out[-2] ne $border && $out[-2] ne $spacer) {
            my $last = pop @out;
            push @out => ($spacer, $last);
        }

        push @out => sprintf($template, @row);
        $split++;
    }

    pop @out while @out && $out[-1] eq $spacer;

    unless (USE_GCS) {
        for my $row (@out) {
            next unless $row =~ m/[[:^ascii:]]/;
            unshift @out => "Unicode::GCString is not installed, table may not display all unicode characters properly";
            last;
        }
    }

    return (@out, $border);
}

sub display {
    my $self = shift;
    my ($fh) = @_;

    my @parts = map "$_\n", $self->render;

    print $fh @parts if $fh;
    print @parts;
}

1;

__END__


=pod

=encoding UTF-8

=head1 NAME

Term::Table - Format a header and rows into a table

=head1 DESCRIPTION

This is used by some failing tests to provide diagnostics about what has gone
wrong. This module is able to format rows of data into tables.

=head1 SYNOPSIS

    use Term::Table;

    my $table = Term::Table->new(
        max_width      => 80,    # Defaults to terminal size
        pad            => 4,     # Extra padding between table and max-width (defaults to 4)
        allow_overflow => 0,     # Default is 0, when off an exception will be thrown if the table is too big
        collapse       => 1,     # Do not show empty columns

        header => ['name', 'age', 'hair color'],
        rows   => [
            ['Fred Flintstone',  2000000, 'black'],
            ['Wilma Flintstone', 1999995, 'red'],
            ...
        ],
    );

    say $_ for $table->render;

This prints a table like this:

    +------------------+---------+------------+
    | name             | age     | hair color |
    +------------------+---------+------------+
    | Fred Flintstone  | 2000000 | black      |
    | Wilma Flintstone | 1999995 | red        |
    | ...              | ...     | ...        |
    +------------------+---------+------------+

=head1 INTERFACE

    use Term::Table;
    my $table = Term::Table->new(...);

=head2 OPTIONS

=over 4

=item header => [ ... ]

If you want a header specify it here.
This takes an arrayref with each columns heading.

=item rows => [ [...], [...], ... ]

This should be an arrayref containing an arrayref per row.

=item collapse => $bool

Use this if you want to hide empty columns, that is any column that has no data
in any row. Having a header for the column will not effect collapse.

=item max_width => $num

Set the maximum width of the table, the table may not be this big, but it will
be no bigger. If none is specified it will attempt to find the width of your
terminal and use that, otherwise it falls back to the terminal width or C<80>.

=item pad => $num

Defaults to C<4>, extra padding for row width calculations.
Default is for legacy support.
Set this to C<0> to turn padding off.

=item allow_overflow => $bool

Defaults to C<0>. If this is off then an exception will be thrown if the table
cannot be made to fit inside the max-width. If this is set to C<1> then the
table will be rendered anyway, larger than max-width, if it is not possible
to stay within the max-width. In other words this turns max-width from a
hard-limit to a soft recommendation.

=item sanitize => $bool

This will sanitize all the data in the table such that newlines, control
characters, and all whitespace except for ASCII 20 C<' '> are replaced with
escape sequences. This prevents newlines, tabs, and similar whitespace from
disrupting the table.

B<Note:> newlines are marked as C<\n>, but a newline is also inserted into the
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

=item show_header => $bool

Set this to false to hide the header. This defaults to true if the header is
set, false if no header is provided.

=item auto_columns => $bool

Set this to true to automatically add columns that are not named in the header.
This defaults to false if a header is provided, and defaults to true when there
is no header.

=item no_collapse => [ $col_num_a, $col_num_b, ... ]

=item no_collapse => [ $col_name_a, $col_name_b, ... ]

=item no_collapse => { $col_num_a => 1, $col_num_b => 1, ... }

=item no_collapse => { $col_name_a => 1, $col_name_b => 1, ... }

Specify (by number and/or name) columns that should not be removed when empty.
The 'name' form only works when a header is specified. There is currently no
protection to insure that names you specify are actually in the header, invalid
names are ignored, patches to fix this will be happily accepted.

=back

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

The source code repository for C<Term-Table> can be found at
L<https://github.com/exodist/Term-Table/>.

=head1 MAINTAINERS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 AUTHORS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright 2016 Chad Granum E<lt>exodist@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See L<https://dev.perl.org/licenses/>

=cut
