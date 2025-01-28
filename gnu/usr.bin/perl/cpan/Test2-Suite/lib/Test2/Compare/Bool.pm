package Test2::Compare::Bool;
use strict;
use warnings;

use Carp qw/confess/;

use base 'Test2::Compare::Base';

our $VERSION = '0.000162';

use Test2::Util::HashBase qw/input/;

# Overloads '!' for us.
use Test2::Compare::Negatable;

sub name {
    my $self = shift;
    my $in = $self->{+INPUT};
    return _render_bool($in);
}

sub operator {
    my $self = shift;
    return '!=' if $self->{+NEGATE};
    return '==';
}

sub verify {
    my $self = shift;
    my %params = @_;
    my ($got, $exists) = @params{qw/got exists/};

    return 0 unless $exists;

    my $want = $self->{+INPUT};

    my $match = ($want xor $got) ? 0 : 1;
    $match = $match ? 0 : 1 if $self->{+NEGATE};

    return $match;
}

sub run {
    my $self = shift;
    my $delta = $self->SUPER::run(@_) or return;

    my $dne = $delta->dne || "";
    unless ($dne eq 'got') {
        my $got = $delta->got;
        $delta->set_got(_render_bool($got));
    }

    return $delta;
}

sub _render_bool {
    my $bool = shift;
    my $name = $bool ? 'TRUE' : 'FALSE';
    my $val = defined $bool ? $bool : 'undef';
    $val = "''" unless length($val);

    return "<$name ($val)>";
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Bool - Compare two values as booleans

=head1 DESCRIPTION

Check if two values have the same boolean result (both true, or both false).

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
