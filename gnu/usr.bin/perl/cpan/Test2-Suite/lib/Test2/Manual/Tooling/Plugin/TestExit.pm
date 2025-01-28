package Test2::Manual::Tooling::Plugin::TestExit;

our $VERSION = '0.000162';

1;

__END__

=head1 NAME

Test2::Manual::Tooling::Plugin::TestExit - How to safely add pre-exit
behaviors.

=head1 DESCRIPTION

This describes the correct/safe way to add pre-exit behaviors to tests via a
custom plugin.

The naive way to attempt this would be to add an C<END { ... }> block. That can
work, and may not cause problems.... On the other hand there are a lot of ways
that can bite you. Describing all the potential problems of an END block, and
how it might conflict with Test2 (Which has its own END block) is beyond the
scope of this document.

=head1 COMPLETE CODE UP FRONT

    package Test2::Plugin::MyPlugin;

    use Test2::API qw{test2_add_callback_exit};

    sub import {
        my $class = shift;

        test2_add_callback_exit(sub {
            my ($ctx, $orig_code, $new_exit_code_ref) = @_;

            return if $orig_code == 42;

            $$new_exit_code_ref = 42;
        });
    }

    1;

=head1 LINE BY LINE

=over 4

=item use Test2::API qw{test2_add_callback_exit};

This imports the C<(test2_add_callback_exit)> callback.

=item test2_add_callback_exit(sub { ... });

This adds our callback to be called before exiting.

=item my ($ctx, $orig_code, $new_exit_code_ref) = @_

The callback gets 3 arguments. First is a context object you may use. The
second is the original exit code of the C<END> block Test2 is using. The third
argument is a scalar reference which you may use to get the current exit code,
or set a new one.

=item return if $orig_code == 42

This is a short-cut to do nothing if the original exit code was already 42.

=item $$new_exit_code_ref = 42

This changes the exit code to 42.

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
