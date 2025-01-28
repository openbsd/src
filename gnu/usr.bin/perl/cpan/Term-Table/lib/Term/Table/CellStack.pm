package Term::Table::CellStack;
use strict;
use warnings;

our $VERSION = '0.018';

use Term::Table::HashBase qw/-cells -idx/;

use List::Util qw/max/;

sub init {
    my $self = shift;
    $self->{+CELLS} ||= [];
}

sub add_cell {
    my $self = shift;
    push @{$self->{+CELLS}} => @_;
}

sub add_cells {
    my $self = shift;
    push @{$self->{+CELLS}} => @_;
}

sub sanitize {
    my $self = shift;
    $_->sanitize(@_) for @{$self->{+CELLS}};
}

sub mark_tail {
    my $self = shift;
    $_->mark_tail(@_) for @{$self->{+CELLS}};
}

my @proxy = qw{
    border_left border_right border_color value_color reset_color
    border_left_width border_right_width
};

for my $meth (@proxy) {
    no strict 'refs';
    *$meth = sub {
        my $self = shift;
        $self->{+CELLS}->[$self->{+IDX}]->$meth;
    };
}

for my $meth (qw{value_width width}) {
    no strict 'refs';
    *$meth = sub {
        my $self = shift;
        return max(map { $_->$meth } @{$self->{+CELLS}});
    };
}

sub next {
    my $self = shift;
    my ($cw) = @_;

    while ($self->{+IDX} < @{$self->{+CELLS}}) {
        my $cell = $self->{+CELLS}->[$self->{+IDX}];

        my $lw = $cell->border_left_width;
        my $rw = $cell->border_right_width;
        my $vw = $cw - $lw - $rw;
        my $it = $cell->break->next($vw);

        return ($it, $vw) if $it;
        $self->{+IDX}++;
    }

    return;
}

sub break { $_[0] }

sub reset {
    my $self = shift;
    $self->{+IDX} = 0;
    $_->reset for @{$self->{+CELLS}};
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Term::Table::CellStack - Combine several cells into one (vertical)

=head1 DESCRIPTION

This package is used to represent a merged-cell in a table (vertical).

=head1 SOURCE

The source code repository for Term-Table can be found at
F<http://github.com/exodist/Term-Table/>.

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

See F<http://dev.perl.org/licenses/>

=cut
