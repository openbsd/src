package Term::Table::Cell;
use strict;
use warnings;

our $VERSION = '0.024';

use Term::Table::LineBreak();
use Term::Table::Util qw/uni_length/;

use List::Util qw/sum/;

use Term::Table::HashBase qw/value border_left border_right _break _widths border_color value_color reset_color/;

my %CHAR_MAP = (
    # Special case, \n should render as \n, but also actually do the newline thing
    "\n" => "\\n\n",

    "\a" => '\\a',
    "\b" => '\\b',
    "\e" => '\\e',
    "\f" => '\\f',
    "\r" => '\\r',
    "\t" => '\\t',
    " "  => ' ',
);

sub init {
    my $self = shift;

    # Stringify
    $self->{+VALUE} = defined $self->{+VALUE} ? "$self->{+VALUE}" : '';
}

sub char_id {
    my $class = shift;
    my ($char) = @_;
    return "\\N{U+" . sprintf("\%X", utf8::native_to_unicode(ord($char))) . "}";
}

sub show_char {
    my $class = shift;
    my ($char, %props) = @_;
    return $char if $props{no_newline} && $char eq "\n";
    return $CHAR_MAP{$char} || $class->char_id($char);
}

sub sanitize {
    my $self = shift;
    $self->{+VALUE} =~ s/([\s\t\p{Zl}\p{C}\p{Zp}])/$self->show_char($1)/ge; # All whitespace except normal space
}

sub mark_tail {
    my $self = shift;
    $self->{+VALUE} =~ s/([\s\t\p{Zl}\p{C}\p{Zp}])$/$1 eq ' ' ? $self->char_id($1) : $self->show_char($1, no_newline => 1)/se;
}

sub value_width {
    my $self = shift;

    my $w = $self->{+_WIDTHS} ||= {};
    return $w->{value} if defined $w->{value};

    my @parts = split /(\n)/, $self->{+VALUE};

    my $max = 0;
    while (@parts) {
        my $text = shift @parts;
        my $sep  = shift @parts || '';
        my $len = uni_length("$text");
        $max = $len if $len > $max;
    }

    return $w->{value} = $max;
}

sub border_left_width {
    my $self = shift;
    $self->{+_WIDTHS}->{left} ||= uni_length($self->{+BORDER_LEFT} || '');
}

sub border_right_width {
    my $self = shift;
    $self->{+_WIDTHS}->{right} ||= uni_length($self->{+BORDER_RIGHT} || '');
}

sub width {
    my $self = shift;
    $self->{+_WIDTHS}->{all} ||= sum(map { $self->$_ } qw/value_width border_left_width border_right_width/);
}

sub break {
    my $self = shift;
    $self->{+_BREAK} ||= Term::Table::LineBreak->new(string => $self->{+VALUE});
}

sub reset {
    my $self = shift;
    delete $self->{+_BREAK};
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Term::Table::Cell - Representation of a cell in a table.

=head1 DESCRIPTION

This package is used to represent a cell in a table.

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
