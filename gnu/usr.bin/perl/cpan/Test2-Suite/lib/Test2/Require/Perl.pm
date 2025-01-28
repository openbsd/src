package Test2::Require::Perl;
use strict;
use warnings;

use base 'Test2::Require';

our $VERSION = '0.000162';

use Test2::Util qw/pkg_to_file/;
use Scalar::Util qw/reftype/;

sub skip {
    my $class = shift;
    my ($ver) = @_;

    return undef if eval "no warnings 'portable'; require $ver; 1";
    my $error = $@;
    return $1 if $error =~ m/^(Perl \S* required)/i;
    die $error;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Require::Perl - Skip the test unless the necessary version of Perl is
installed.

=head1 DESCRIPTION

Sometimes you have tests that are nice to run, but depend on a certain version
of Perl. This package lets you run the test conditionally, depending on if the
correct version of Perl is available.

=head1 SYNOPSIS

    # Skip the test unless perl 5.10 or greater is installed.
    use Test2::Require::Perl 'v5.10';

    # Enable 5.10 features.
    use v5.10;

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
