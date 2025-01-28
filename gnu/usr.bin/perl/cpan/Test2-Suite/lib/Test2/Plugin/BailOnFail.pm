package Test2::Plugin::BailOnFail;
use strict;
use warnings;

our $VERSION = '0.000162';

use Test2::API qw/test2_add_callback_context_release/;

my $LOADED = 0;
sub import {
    return if $LOADED++;

    test2_add_callback_context_release(sub {
        my $ctx = shift;
        return if $ctx->hub->is_passing;
        $ctx->bail("(Bail On Fail)");
    });
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Plugin::BailOnFail - Automatically bail out of testing on the first test
failure.

=head1 DESCRIPTION

This module will issue a bailout event after the first test failure. This will
prevent your tests from continuing. The bailout runs when the context is
released; that is, it will run when the test function you are using, such as
C<ok()>, returns. This gives the tools the ability to output any extra
diagnostics they may need.

=head1 SYNOPSIS

    use Test2::V0;
    use Test2::Plugin::BailOnFail;

    ok(1, "pass");
    ok(0, "fail");
    ok(1, "Will not run");

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
