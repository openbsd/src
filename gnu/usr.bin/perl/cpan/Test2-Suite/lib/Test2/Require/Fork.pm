package Test2::Require::Fork;
use strict;
use warnings;

use base 'Test2::Require';

our $VERSION = '0.000162';

use Test2::Util qw/CAN_FORK/;

sub skip {
    return undef if CAN_FORK;
    return "This test requires a perl capable of forking.";
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Require::Fork - Skip a test file unless the system supports forking

=head1 DESCRIPTION

It is fairly common to write tests that need to fork. Not all systems support
forking. This library does the hard work of checking if forking is supported on
the current system. If forking is not supported then this will skip all tests
and exit true.

=head1 SYNOPSIS

    use Test2::Require::Fork;

    ... Code that forks ...

=head1 EXPLANATION

Checking if the current system supports forking is not simple. Here is an
example of how to do it:

    use Config;

    sub CAN_FORK {
        return 1 if $Config{d_fork};

        # Some platforms use ithreads to mimic forking
        return 0 unless $^O eq 'MSWin32' || $^O eq 'NetWare';
        return 0 unless $Config{useithreads};
        return 0 unless $Config{ccflags} =~ /-DPERL_IMPLICIT_SYS/;

        # Threads are not reliable before 5.008001
        return 0 unless $] >= 5.008001;

        # Devel::Cover currently breaks with threads
        return 0 if $INC{'Devel/Cover.pm'};
        return 1;
    }

Duplicating this non-trivial code in all tests that need to fork is error-prone. It is
easy to forget bits, or get it wrong. On top of these checks, you also need to
tell the harness that no tests should run and why.

=head1 SEE ALSO

=over 4

=item L<Test2::Require::CanReallyfork>

Similar to this module, but will skip on any perl that only has fork emulation.

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
