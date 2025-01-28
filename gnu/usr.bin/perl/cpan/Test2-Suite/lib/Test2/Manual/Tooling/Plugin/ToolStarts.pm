package Test2::Manual::Tooling::Plugin::ToolStarts;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Tooling::Plugin::ToolStarts - How to add behaviors that occur
when a tool starts work.

=head1 DESCRIPTION

This tutorial will help you write plugins that have behavior when a tool
starts. All tools should start by acquiring a context object. This tutorial
shows you the hooks you can use to take advantage of the context acquisition.

=head1 COMPLETE CODE UP FRONT

    package Test2::Plugin::MyPlugin;

    use Test2::API qw{
        test2_add_callback_context_init
        test2_add_callback_context_acquire
    };

    sub import {
        my $class = shift;

        # Let us know every time a tool requests a context, and give us a
        # chance to modify the parameters before we find it.
        test2_add_callback_context_acquire(sub {
            my $params_ref = shift;

            print "A tool has requested the context\n";
        });

        # Callback every time a new context is created, not called if an
        # existing context is found.
        test2_add_callback_context_init(sub {
            my $ctx_ref = shift;

            print "A new context was created\n";
        });
    }

    1;

=head1 LINE BY LINE

=over 4

=item use Test2::API qw{test2_add_callback_context_init test2_add_callback_context_acquire};

This imports the C<test2_add_callback_context_init()> and
C<test2_add_callback_context_acquire()> callbacks.

=item test2_add_callback_context_acquire(sub { ... })

This is where we add our callback for context acquisition. Every time
C<Test2::API::context()> is called the callback will be run.

=item my $params_ref = shift

In the test2_add_callback_context_acquire() callbacks we get exactly 1
argument, a reference to the parameters that C<context()> will use to find the
context.

=item print "A tool has requested the context\n"

Print a notification whenever a tool asks for a context.

=item test2_add_callback_context_init(sub { ... })

Add our context init callback. These callbacks are triggered whenever a
completely new context is created. This is not called if an existing context is
found. In short this only fires off for the top level tool, not nested tools.

=item my $ctx_ref = shift

The coderefs for test2_add_callback_context_init() will receive exactly 1
argument, the newly created context.

=item print "A new context was created\n"

Print a notification whenever a new context is created.

=back

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
