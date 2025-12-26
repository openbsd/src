package Term::Table::LineBreak;
use strict;
use warnings;

our $VERSION = '0.024';

use Carp qw/croak/;
use Scalar::Util qw/blessed/;
use Term::Table::Util qw/uni_length/;

use Term::Table::HashBase qw/string gcstring _len _parts idx/;

sub init {
    my $self = shift;

    croak "string is a required attribute"
        unless defined $self->{+STRING};
}

sub columns { uni_length($_[0]->{+STRING}) }

sub break {
    my $self = shift;
    my ($len) = @_;

    $self->{+_LEN} = $len;

    $self->{+IDX} = 0;
    my $str = $self->{+STRING} . ""; # Force stringification

    my @parts;
    my @chars = split //, $str;
    while (@chars) {
        my $size = 0;
        my $part = '';
        until ($size == $len) {
            my $char = shift @chars;
            $char = '' unless defined $char;

            my $l = uni_length("$char");
            last unless $l;

            last if $char eq "\n";

            if ($size + $l > $len) {
                unshift @chars => $char;
                last;
            }

            $size += $l;
            $part .= $char;
        }

        # If we stopped just before a newline, grab it
        shift @chars if $size == $len && @chars && $chars[0] eq "\n";

        until ($size == $len) {
            $part .= ' ';
            $size += 1;
        }
        push @parts => $part;
    }

    $self->{+_PARTS} = \@parts;
}

sub next {
    my $self = shift;

    if (@_) {
        my ($len) = @_;
        $self->break($len) if !$self->{+_LEN} || $self->{+_LEN} != $len;
    }
    else {
        croak "String has not yet been broken"
            unless $self->{+_PARTS};
    }

    my $idx   = $self->{+IDX}++;
    my $parts = $self->{+_PARTS};

    return undef if $idx >= @$parts;
    return $parts->[$idx];
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Term::Table::LineBreak - Break up lines for use in tables.

=head1 DESCRIPTION

This is meant for internal use. This package takes long lines of text and
splits them so that they fit in table rows.

=head1 SYNOPSIS

    use Term::Table::LineBreak;

    my $lb = Term::Table::LineBreak->new(string => $STRING);

    $lb->break($SIZE);
    while (my $part = $lb->next) {
        ...
    }

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
