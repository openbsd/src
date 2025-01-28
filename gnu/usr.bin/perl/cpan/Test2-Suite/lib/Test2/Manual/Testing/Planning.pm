package Test2::Manual::Testing::Planning;
use strict;
use warnings;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Testing::Planning - The many ways to set a plan.

=head1 DESCRIPTION

This tutorial covers the many ways of setting a plan.

=head1 TEST COUNT

The C<plan()> function is provided by L<Test2::Tools::Basic>. This function lets
you specify an exact number of tests to run. This can be done at the start of
testing, or at the end. This cannot be done partway through testing.

    use Test2::Tools::Basic;
    plan(10); # 10 tests expected

    ...

=head1 DONE TESTING

The C<done_testing()> function is provided by L<Test2::Tools::Basic>. This
function will automatically set the plan to the number of tests that were run.
This must be used at the very end of testing.

    use Test2::Tools::Basic;

    ...

    done_testing();

=head1 SKIP ALL

The C<skip_all()> function is provided by L<Test2::Tools::Basic>. This function
will set the plan to C<0>, and exit the test immediately. You may provide a skip
reason that explains why the test should be skipped.

    use Test2::Tools::Basic;
    skip_all("This test will not run here") if ...;

    ...

=head1 CUSTOM PLAN EVENT

A plan is simply an L<Test2::Event::Plan> event that gets sent to the current
hub. You could always write your own tool to set the plan.

    use Test2::API qw/context/;

    sub set_plan {
        my $count = @_;

        my $ctx = context();
        $ctx->send_event('Plan', max => $count);
        $ctx->release;

        return $count;
    }

=head1 SEE ALSO

L<Test2::Manual> - Primary index of the manual.

=head1 SOURCE

The source code repository for Test2-Manual can be found at
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
