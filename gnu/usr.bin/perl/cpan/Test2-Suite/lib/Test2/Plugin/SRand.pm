package Test2::Plugin::SRand;
use strict;
use warnings;

our $VERSION = '0.000162';

use Carp qw/carp/;

use Test2::API qw{
    context
    test2_add_callback_post_load
    test2_add_callback_exit
    test2_stack
};

my $ADDED_HOOK = 0;
my $SEED;
my $FROM;

sub seed { $SEED }
sub from { $FROM }

sub import {
    my $class = shift;

    carp "SRand loaded multiple times, re-seeding rand"
        if defined $SEED;

    if (@_ == 1) {
        ($SEED) = @_;
        $FROM = 'import arg';
    }
    elsif (@_ == 2 and $_[0] eq 'seed') {
        $SEED = $_[1];
        $FROM = 'import arg';
    }
    elsif(exists $ENV{T2_RAND_SEED}) {
        $SEED = $ENV{T2_RAND_SEED};
        $FROM = 'environment variable';
    }
    else {
        my @ltime = localtime;
        # Yes, this would be an awful seed if you actually wanted randomness.
        # The idea here is that we want "random" behavior to be predictable
        # within a given day. This allows you to reproduce failures that may or
        # may not happen due to randomness.
        $SEED = sprintf('%04d%02d%02d', 1900 + $ltime[5], 1 + $ltime[4], $ltime[3]);
        $FROM = 'local date';
    }

    $SEED = 0 unless $SEED;
    srand($SEED);

    if ($ENV{HARNESS_IS_VERBOSE} || !$ENV{HARNESS_ACTIVE}) {
        # If the harness is verbose then just display the message for all to
        # see. It is nice info and they already asked for noisy output.

        test2_add_callback_post_load(sub {
            test2_stack()->top; # Ensure we have at least 1 hub.
            my ($hub) = test2_stack()->all;
            $hub->send(
                Test2::Event::Note->new(
                    trace => Test2::Util::Trace->new(frame => [__PACKAGE__, __FILE__, __LINE__, 'SRAND']),
                    message => "Seeded srand with seed '$SEED' from $FROM.",
                )
            );
        });
    }
    elsif (!$ADDED_HOOK++) {
        # The seed can be important for debugging, so if anything is wrong we
        # should output the seed message as a diagnostics message. This must be
        # done at the very end, even later than a hub hook.
        test2_add_callback_exit(
            sub {
                my ($ctx, $real, $new) = @_;

                $ctx->diag("Seeded srand with seed '$SEED' from $FROM.")
                    if $real
                    || ($new && $$new)
                    || !$ctx->hub->is_passing;
            }
        );
    }
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Plugin::SRand - Control the random seed for more controlled test
environments.

=head1 DESCRIPTION

This module gives you control over the random seed used for your unit tests. In
some testing environments the random seed can play a major role in results.

The default configuration for this module will seed srand with the local date.
Using the date as the seed means that on any given day the random seed will
always be the same, this means behavior will not change from run to run on a
given day. However the seed is different on different days allowing you to be
sure the code still works with actual randomness.

The seed is printed for you on failure, or when the harness is verbose. You can
use the C<T2_RAND_SEED> environment variable to specify the seed. You can also
provide a specific seed as a load-time argument to the plugin.

=head1 SYNOPSIS

Loading the plugin is easy, and the defaults are sane:

    use Test2::Plugin::SRand;

Custom seed:

    use Test2::Plugin::SRand seed => 42;

=head1 NOTE ON LOAD ORDER

If you use this plugin you probably want to use it as the first, or near-first
plugin. C<srand> is not called until the plugin is loaded, so other plugins
loaded first may already be making use of random numbers before your seed
takes effect.

=head1 CAVEATS

When srand is on (default) it can cause problems with things like L<File::Temp>
which will end up attempting the same "random" filenames for every test process
started on a given day (or sharing the same seed).

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
