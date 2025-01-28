package Test2::Util::Times;
use strict;
use warnings;

use List::Util qw/sum/;

our $VERSION = '0.000162';

our @EXPORT_OK = qw/render_bench render_duration/;
use base 'Exporter';

sub render_duration {
    my $time;
    if (@_ == 1) {
        ($time) = @_;
    }
    else {
        my ($start, $end) = @_;
        $time = $end - $start;
    }

    return sprintf('%1.5fs', $time) if $time < 10;
    return sprintf('%2.4fs', $time) if $time < 60;

    my $msec  = substr(sprintf('%0.2f', $time - int($time)), -2, 2);
    my $secs  = $time % 60;
    my $mins  = int($time / 60) % 60;
    my $hours = int($time / 60 / 60) % 24;
    my $days  = int($time / 60 / 60 / 24);

    my @units = (qw/d h m/, '');

    my $duration = '';
    for my $t ($days, $hours, $mins, $secs) {
        my $u = shift @units;
        next unless $t || $duration;
        $duration = join ':' => grep { length($_) } $duration, sprintf('%02u%s', $t, $u);
    }

    $duration ||= '0';
    $duration .= ".$msec" if int($msec);
    $duration .= 's';

    return $duration;
}

sub render_bench {
    my ($start, $end, $user, $system, $cuser, $csystem) = @_;

    my $duration = render_duration($start, $end);

    my $bench = sprintf(
        "%s on wallclock (%5.2f usr %5.2f sys + %5.2f cusr %5.2f csys = %5.2f CPU)",
        $duration, $user, $system, $cuser, $csystem, sum($user, $system, $cuser, $csystem),
    );
    $bench =~ s/\s+/ /g;
    $bench =~ s/(\(|\))\s+/$1/g;

    return $bench;
}

1;

=pod

=encoding UTF-8

=head1 NAME

Test2::Util::Times - Format timing/benchmark information.

=head1 DESCRIPTION

This modules exports tools for rendering timing data at the end of tests.

=head1 EXPORTS

All exports are optional. You must specify subs to import.

=over 4

=item $str = render_bench($start, $end, $user, $system, $cuser, $csystem)

=item $str = render_bench($start, time(), times())

This will produce a string like one of these (Note these numbers are completely
made up). I<Which string is used depends on the time elapsed.>

    0.12345s on wallclock (0.05 usr 0.00 sys + 0.00 cusr 0.00 csys = 0.05 CPU)

    11.1234s on wallclock (0.05 usr 0.00 sys + 0.00 cusr 0.00 csys = 0.05 CPU)

    01m:54.45s on wallclock (0.05 usr 0.00 sys + 0.00 cusr 0.00 csys = 0.05 CPU)

    18h:22m:54.45s on wallclock (0.05 usr 0.00 sys + 0.00 cusr 0.00 csys = 0.05 CPU)

    04d:18h:22m:54.45s on wallclock (0.05 usr 0.00 sys + 0.00 cusr 0.00 csys = 0.05 CPU)

The first 2 arguments are the C<$start> and C<$end> times in seconds (as
returned by C<time()> or C<Time::HiRes::time()>).

The last 4 arguments are timing information as returned by the C<times()>
function.

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
