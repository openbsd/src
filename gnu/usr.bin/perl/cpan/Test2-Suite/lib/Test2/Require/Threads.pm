package Test2::Require::Threads;
use strict;
use warnings;

use base 'Test2::Require';

our $VERSION = '0.000162';

use Test2::Util qw/CAN_THREAD/;

sub skip {
    return undef if CAN_THREAD;
    return "This test requires a perl capable of threading.";
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Require::Threads - Skip a test file unless the system supports threading

=head1 DESCRIPTION

It is fairly common to write tests that need to use threads. Not all systems
support threads. This library does the hard work of checking if threading is
supported on the current system. If threading is not supported then this will
skip all tests and exit true.

=head1 SYNOPSIS

    use Test2::Require::Threads;

    ... Code that uses threads ...

=head1 EXPLANATION

Checking if the current system supports threading is not simple, here is an
example of how to do it:

    use Config;

    sub CAN_THREAD {
        # Threads are not reliable before 5.008001
        return 0 unless $] >= 5.008001;
        return 0 unless $Config{'useithreads'};

        # Devel::Cover currently breaks with threads
        return 0 if $INC{'Devel/Cover.pm'};
        return 1;
    }

Duplicating this non-trivial code in all tests that need to use threads is
error-prone. It is easy to forget bits, or get it wrong. On top of these checks you
also need to tell the harness that no tests should run and why.

=head1 SEE ALSO

=over 4

=item L<Test2::Require::CanFork>

Skip the test file if the system does not support forking.

=item L<Test2>

Test2::Require::Threads uses L<Test2> under the hood.

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
