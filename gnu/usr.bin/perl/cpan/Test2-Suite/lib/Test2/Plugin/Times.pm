package Test2::Plugin::Times;
use strict;
use warnings;

use Test2::Util::Times qw/render_bench render_duration/;

use Test2::API qw{
    test2_add_callback_exit
};

use Time::HiRes qw/time/;

our $VERSION = '0.000162';

my $ADDED_HOOK = 0;
my $START;
sub import {
    return if $ADDED_HOOK++;

    $START = time;
    test2_add_callback_exit(\&send_time_event);
}

sub send_time_event {
    my ($ctx, $real, $new) = @_;
    my $stop  = time;
    my @times = times();

    my $summary  = render_bench($START, $stop, @times);
    my $duration = render_duration($START, $stop);

    my $e = $ctx->send_ev2(
        about => {package => __PACKAGE__, details => $summary},
        info  => [{tag => 'TIME', details => $summary}],
        times => {
            details => $summary,
            start  => $START,
            stop   => $stop,
            user   => $times[0],
            sys    => $times[1],
            cuser  => $times[2],
            csys   => $times[3],
        },
        harness_job_fields => [
            {name => "time_duration", details => $duration},
            {name => "time_user",     details => $times[0]},
            {name => "time_sys",      details => $times[1]},
            {name => "time_cuser",    details => $times[2]},
            {name => "time_csys",     details => $times[3]},
        ],
    );
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Plugin::Times - Output timing data at the end of the test.

=head1 CAVEAT

It is important to note that this timing data does not include global
destruction. This data is only collected up until the point done_testing() is
called. If your program takes time for END blocks, garbage collection, and
similar, then this timing data will fall short of reality.

=head1 DESCRIPTION

This plugin will output a diagnostics message at the end of testing that tells
you how much time elapsed, and how hard the system worked on the test.

This will produce a string like one of these (Note these numbers are completely
made up). I<Which string is used depends on the time elapsed.>

    0.12345s on wallclock (0.05 usr 0.00 sys + 0.00 cusr 0.00 csys = 0.05 CPU)

    11.1234s on wallclock (0.05 usr 0.00 sys + 0.00 cusr 0.00 csys = 0.05 CPU)

    01m:54.45s on wallclock (0.05 usr 0.00 sys + 0.00 cusr 0.00 csys = 0.05 CPU)

    18h:22m:54.45s on wallclock (0.05 usr 0.00 sys + 0.00 cusr 0.00 csys = 0.05 CPU)

    04d:18h:22m:54.45s on wallclock (0.05 usr 0.00 sys + 0.00 cusr 0.00 csys = 0.05 CPU)

=head1 SYNOPSIS

    use Test2::Plugin::Times;

This is also useful at the command line for 1-time use:

    $ perl -MTest2::Plugin::Times path/to/test.t

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
