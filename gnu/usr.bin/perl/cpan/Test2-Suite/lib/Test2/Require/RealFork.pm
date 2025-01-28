package Test2::Require::RealFork;
use strict;
use warnings;

use base 'Test2::Require';

our $VERSION = '0.000162';

use Test2::Util qw/CAN_REALLY_FORK/;

sub skip {
    return undef if CAN_REALLY_FORK;
    return "This test requires a perl capable of true forking.";
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Require::RealFork - Skip a test file unless the system supports true
forking

=head1 DESCRIPTION

It is fairly common to write tests that need to fork. Not all systems support
forking. This library does the hard work of checking if forking is supported on
the current system. If forking is not supported then this will skip all tests
and exit true.

=head1 SYNOPSIS

    use Test2::Require::RealFork;

    ... Code that forks ...

=head1 SEE ALSO

=over 4

=item L<Test2::Require::Canfork>

Similar to this module, but will allow fork emulation.

=item L<Test2::Require::CanThread>

Skip the test file if the system does not support threads.

=back

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
