package Test2::Manual::Tooling::Plugin::ToolCompletes;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Tooling::Plugin::ToolCompletes - How to add behaviors that occur
when a tool completes work.

=head1 DESCRIPTION

This tutorial helps you understand how to add behaviors that occur when a tool
is done with its work. All tools need to acquire and then release a context,
for this tutorial we make use of the release hooks that are called every time a
tool releases the context object.

=head1 COMPLETE CODE UP FRONT

    package Test2::Plugin::MyPlugin;

    use Test2::API qw{test2_add_callback_context_release};

    sub import {
        my $class = shift;

        test2_add_callback_context_release(sub {
            my $ctx_ref = shift;

            print "Context was released\n";
        });
    }

    1;

=head1 LINE BY LINE

=over 4

=item use Test2::API qw{test2_add_callback_context_release};

This imports the C<test2_add_callback_context_release()> callback.

=item test2_add_callback_context_release(sub { ... })

=item my $ctx_ref = shift

The coderefs for test2_add_callback_context_release() will receive exactly 1
argument, the context being released.

=item print "Context was released\n"

Print a notification whenever the context is released.

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
