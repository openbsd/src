package Test2::Compare::Undef;
use strict;
use warnings;

use Carp qw/confess/;

use base 'Test2::Compare::Base';

our $VERSION = '0.000162';

use Test2::Util::HashBase;

# Overloads '!' for us.
use Test2::Compare::Negatable;

sub name { '<UNDEF>' }

sub operator {
    my $self = shift;

    return 'IS NOT' if $self->{+NEGATE};
    return 'IS';
}

sub verify {
    my $self = shift;
    my %params = @_;
    my ($got, $exists) = @params{qw/got exists/};

    return 0 unless $exists;

    return !defined($got) unless $self->{+NEGATE};
    return defined($got);
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Undef - Check that something is undefined

=head1 DESCRIPTION

Make sure something is undefined in a comparison. You can also check that
something is defined.

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
