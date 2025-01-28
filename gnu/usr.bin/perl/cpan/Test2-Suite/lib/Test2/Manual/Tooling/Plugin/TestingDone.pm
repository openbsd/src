package Test2::Manual::Tooling::Plugin::TestingDone;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Tooling::Plugin::TestingDone - Run code when the test file is
finished, or when done_testing is called.

=head1 DESCRIPTION

This is a way to add behavior to the end of a test file. This code is run
either when done_testing() is called, or when the test file has no more
run-time code to run.

When triggered by done_testing() this will be run BEFORE the plan is calculated
and sent. This means it IS safe to make test assertions in this callback.

=head1 COMPLETE CODE UP FRONT

    package Test2::Plugin::MyPlugin;

    use Test2::API qw{test2_add_callback_testing_done};

    sub import {
        my $class = shift;

        test2_add_callback_testing_done(sub {
            ok(!$some_global, '$some_global was not set');
            print "The test file is done, or done_testing was just called\n"
        });
    }

    1;

=head1 LINE BY LINE

=over 4

=item use Test2::API qw{test2_add_callback_testing_done};

This imports the C<test2_add_callback_testing_done()> callback.

=item test2_add_callback_testing_done(sub { ... });

This adds our callback to be called when testing is done.

=item ok(!$some_global, '$some_global was not set')

It is safe to make assertions in this type of callback. This code simply
asserts that some global was never set over the course of the test.

=item print "The test file is done, or done_testing was just called\n"

This prints a message when the callback is run.

=back

=head1 UNDER THE HOOD

Before test2_add_callback_testing_done() this kind of thing was still possible,
but it was hard to get right, here is the code to do it:

    test2_add_callback_post_load(sub {
        my $stack = test2_stack();

        # Insure we have at least one hub, but we do not necessarily want the
        # one this returns.
        $stack->top;

        # We want the root hub, not the top one.
        my ($root) = Test2::API::test2_stack->all;

        # Make sure the hub does not believe nothing has happened.
        $root->set_active(1);

        # Now we can add our follow-up code
        $root->follow_up(sub {
            # Your callback code here
        });
    });

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
