package Test2::Compare::Pattern;
use strict;
use warnings;

use base 'Test2::Compare::Base';

our $VERSION = '0.000162';

use Test2::Util::HashBase qw/pattern stringify_got/;

# Overloads '!' for us.
use Test2::Compare::Negatable;

use Carp qw/croak/;

sub init {
    my $self = shift;

    croak "'pattern' is a required attribute" unless $self->{+PATTERN};

    $self->{+STRINGIFY_GOT} ||= 0;

    $self->SUPER::init();
}

sub name { shift->{+PATTERN} . "" }
sub operator { shift->{+NEGATE} ? '!~' : '=~' }

sub verify {
    my $self = shift;
    my %params = @_;
    my ($got, $exists) = @params{qw/got exists/};

    return 0 unless $exists;
    return 0 unless defined($got);
    return 0 if ref $got && !$self->stringify_got;

    return $got !~ $self->{+PATTERN}
        if $self->{+NEGATE};

    return $got =~ $self->{+PATTERN};
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Compare::Pattern - Use a pattern to validate values in a deep
comparison.

=head1 DESCRIPTION

This allows you to use a regex to validate a value in a deep comparison.
Sometimes a value just needs to look right, it may not need to be exact. An
example is a memory address that might change from run to run.

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
