package Test2::Compare::Isa;
use strict;
use warnings;

use Carp qw/confess/;
use Scalar::Util qw/blessed/;

use base 'Test2::Compare::Base';

our $VERSION = '0.000162';

use Test2::Util::HashBase qw/input/;

# Overloads '!' for us.
use Test2::Compare::Negatable;

sub init {
    my $self = shift;
    confess "input must be defined for 'Isa' check"
        unless defined $self->{+INPUT};

    $self->SUPER::init(@_);
}

sub name {
    my $self = shift;
    my $in = $self->{+INPUT};
    return "$in";
}

sub operator {
    my $self = shift;
    return '!isa' if $self->{+NEGATE};
    return 'isa';
}

sub verify {
    my $self = shift;
    my %params = @_;
    my ($got, $exists) = @params{qw/got exists/};

    return 0 unless $exists;

    my $input = $self->{+INPUT};
    my $negate = $self->{+NEGATE};
    my $isa = blessed($got) && $got->isa($input);

    return !$isa if $negate;
    return $isa;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Isa - Check if the value is an instance of the class.

=head1 DESCRIPTION

This is used to check if the got value is an instance of the expected class.

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

=item TOYAMA Nao E<lt>nanto@moon.email.ne.jpE<gt>

=back

=head1 COPYRIGHT

Copyright 2018 Chad Granum E<lt>exodist@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://dev.perl.org/licenses/>

=cut
