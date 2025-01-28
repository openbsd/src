package Test2::Plugin::ExitSummary;
use strict;
use warnings;

our $VERSION = '0.000162';

use Test2::API qw/test2_add_callback_exit/;

my $ADDED_HOOK = 0;
sub import { test2_add_callback_exit(\&summary) unless $ADDED_HOOK++ }

sub active { $ADDED_HOOK }

sub summary {
    my ($ctx, $real, $new) = @_;

    # Avoid double-printing diagnostics if Test::Builder already loaded.
    return if $INC{'Test/Builder.pm'};

    my $hub    = $ctx->hub;
    my $plan   = $hub->plan;
    my $count  = $hub->count;
    my $failed = $hub->failed;

    $ctx->diag('No tests run!') if !$count && (!$plan || $plan ne 'SKIP');
    $ctx->diag('Tests were run but no plan was declared and done_testing() was not seen.')
        if $count && !$plan;

    $ctx->diag("Looks like your test exited with $real after test #$count.")
        if $real;

    $ctx->diag("Did not follow plan: expected $plan, ran $count.")
        if $plan && $plan =~ m/^[0-9]+$/ && defined $count && $count != $plan;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Plugin::ExitSummary - Add extra diagnostics on failure at the end of the
test.

=head1 DESCRIPTION

This will provide some diagnostics after a failed test. These diagnostics can
range from telling you how you deviated from your plan, warning you if there
was no plan, etc. People used to L<Test::More> generally expect these
diagnostics.

=head1 SYNOPSIS

    use Test2::Plugin::ExitSummary;

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
