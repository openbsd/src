package Test2::Require::EnvVar;
use strict;
use warnings;

use Carp qw/confess/;
use base 'Test2::Require';

our $VERSION = '0.000162';

sub skip {
    my $class = shift;
    my ($var) = @_;
    confess "no environment variable specified" unless $var;
    return undef if $ENV{$var};
    return "This test only runs if the \$$var environment variable is set";
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Require::EnvVar - Only run a test when a specific environment variable
is set.

=head1 DESCRIPTION

It is common practice to write tests that are only run when an environment
variable is set. This module automates the (admittedly trivial) work of creating
such a test.

=head1 SYNOPSIS

    use Test2::Require::EnvVar 'SOME_VAR';

    ...

    done_testing;

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
