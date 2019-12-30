#!./perl

use strict;
use warnings;

use Config;
use Test::More 0.96;
use Time::Local
    qw( timegm timelocal timegm_modern timelocal_modern timegm_nocheck timelocal_nocheck );

# Use 3 days before the start of the epoch because with Borland on
# Win32 it will work for -3600 _if_ your time zone is +01:00 (or
# greater).
my $neg_epoch_ok
    = $^O eq 'VMS' ? 0 : defined( ( localtime(-259200) )[0] ) ? 1 : 0;

my $large_epoch_ok = eval { ( gmtime 2**40 )[5] == 34912 };

{
    my %tests = _valid_time_tests();
    for my $group ( sort keys %tests ) {
        subtest(
            $group,
            sub { _test_group( $tests{$group} ) },
        );
    }
}

sub _valid_time_tests {
    my %tests = (
        'simple times' => [
            [ 1970, 1,  2,  0,  0,  0 ],
            [ 1980, 2,  28, 12, 0,  0 ],
            [ 1980, 2,  29, 12, 0,  0 ],
            [ 1999, 12, 31, 23, 59, 59 ],
            [ 2000, 1,  1,  0,  0,  0 ],
            [ 2010, 10, 12, 14, 13, 12 ],
        ],
        'leap days' => [
            [ 2020, 2, 29, 12, 59, 59 ],
            [ 2030, 7, 4,  17, 7,  6 ],
        ],
        'non-integer seconds' => [
            [ 2010, 10, 12, 14, 13, 12.1 ],
            [ 2010, 10, 12, 14, 13, 59.1 ],
        ],
    );

    # The following test fails on a surprising number of systems
    # so it is commented out. The end of the Epoch for a 32-bit signed
    # implementation of time_t should be Jan 19, 2038  03:14:07 UTC.
    #  [2038,  1, 17, 23, 59, 59],     # last full day in any tz

    # more than 2**31 time_t - requires a 64bit safe localtime/gmtime
    $tests{'greater than 2**31 seconds'} = [ [ 2258, 8, 11, 1, 49, 17 ] ]
        if $] >= 5.012000;

    # use vmsish 'time' makes for oddness around the Unix epoch
    $tests{'simple times'}[0][2]++
        if $^O eq 'VMS';

    $tests{'negative epoch'} = [
        [ 1969, 12, 31, 16, 59, 59 ],
        [ 1950, 4,  12, 9,  30, 31 ],
    ] if $neg_epoch_ok;

    return %tests;
}

sub _test_group {
    my $group = shift;

    for my $vals ( @{$group} ) {
        my ( $year, $mon, $mday, $hour, $min, $sec ) = @{$vals};
        $mon--;

        # 1970 test on VOS fails
        next if $^O eq 'vos' && $year == 1970;

        for my $sub (qw( timelocal timelocal_nocheck timelocal_modern )) {
            subtest(
                $sub,
                sub {
                    my $time = __PACKAGE__->can($sub)
                        ->( $sec, $min, $hour, $mday, $mon, $year );

                    is_deeply(
                        [ ( localtime($time) )[ 0 .. 5 ] ],
                        [ int($sec), $min, $hour, $mday, $mon, $year - 1900 ],
                        "timelocal for @{$vals}"
                    );
                },
            );
        }

        for my $sub (qw( timegm timegm_nocheck timegm_modern )) {
            subtest(
                $sub,
                sub {
                    my $time = __PACKAGE__->can($sub)
                        ->( $sec, $min, $hour, $mday, $mon, $year );

                    is_deeply(
                        [ ( gmtime($time) )[ 0 .. 5 ] ],
                        [ int($sec), $min, $hour, $mday, $mon, $year - 1900 ],
                        "timegm for @{$vals}"
                    );
                },
            );
        }
    }
}

subtest(
    'bad times',
    sub {
        my %bad = (
            'month too large'  => [ 1995, 13, 1,  1,  1,  1 ],
            'day too large'    => [ 1995, 2,  30, 1,  1,  1 ],
            'hour too large'   => [ 1995, 2,  10, 25, 1,  1 ],
            'minute too large' => [ 1995, 2,  10, 1,  60, 1 ],
            'second too large' => [ 1995, 2,  10, 1,  1,  60 ],
        );

        for my $key ( sort keys %bad ) {
            subtest(
                $key,
                sub {
                    my ( $year, $mon, $mday, $hour, $min, $sec )
                        = @{ $bad{$key} };
                    $mon--;

                    local $@ = undef;
                    eval { timegm( $sec, $min, $hour, $mday, $mon, $year ) };

                    like(
                        $@, qr/.*out of range.*/,
                        "invalid time caused an error - @{$bad{$key}}"
                    );
                }
            );
        }
    },
);

subtest(
    'diff between two calls',
    sub {
        is(
            timelocal( 0, 0, 1, 1, 0, 90 ) - timelocal( 0, 0, 0, 1, 0, 90 ),
            3600,
            'one hour difference between two calls to timelocal'
        );

        is(
                  timelocal( 1, 2, 3, 1, 0, 100 )
                - timelocal( 1, 2, 3, 31, 11, 99 ),
            24 * 3600,
            'one day difference between two calls to timelocal'
        );

        # Diff beween Jan 1, 1980 and Mar 1, 1980 = (31 + 29 = 60 days)
        is(
            timegm( 0, 0, 0, 1, 2, 80 ) - timegm( 0, 0, 0, 1, 0, 80 ),
            60 * 24 * 3600,
            '60 day difference between two calls to timegm'
        );
    },
);

subtest(
    'DST transition bug - https://rt.perl.org/Ticket/Display.html?id=19393',
    sub {
        # At a DST transition, the clock skips forward, eg from 01:59:59 to
        # 03:00:00. In this case, 02:00:00 is an invalid time, and should be
        # treated like 03:00:00 rather than 01:00:00 - negative zone offsets
        # used to do the latter.
        {
            my $hour = ( localtime( timelocal( 0, 0, 2, 7, 3, 102 ) ) )[2];

            # testers in US/Pacific should get 3,
            # other testers should get 2
            ok( $hour == 2 || $hour == 3, 'hour should be 2 or 3' );
        }
    },
);

subtest(
    'Time::Local::_is_leap_year',
    sub {
        my @years = (
            [ 1900 => 0 ],
            [ 1947 => 0 ],
            [ 1996 => 1 ],
            [ 2000 => 1 ],
            [ 2100 => 0 ],
        );

        for my $p (@years) {
            my ( $year, $is_leap_year ) = @$p;

            my $string = $is_leap_year ? 'is' : 'is not';
            ## no critic (Subroutines::ProtectPrivateSubs)
            is(
                Time::Local::_is_leap_year($year), $is_leap_year,
                "$year $string a leap year"
            );
        }
    }
);

subtest(
    'negative epochs',
    sub {
        plan skip_all => 'this platform does not support negative epochs.'
            unless $neg_epoch_ok;

        local $@ = undef;
        eval { timegm( 0, 0, 0, 29, 1, 1900 ) };
        like(
            $@, qr/Day '29' out of range 1\.\.28/,
            'does not accept leap day in 1900'
        );

        local $@ = undef;
        eval { timegm( 0, 0, 0, 29, 1, 200 ) };
        like(
            $@, qr/Day '29' out of range 1\.\.28/,
            'does not accept leap day in 2100 (year passed as 200)'
        );

        local $@ = undef;
        eval { timegm( 0, 0, 0, 29, 1, 0 ) };
        is(
            $@, q{},
            'no error with leap day of 2000 (year passed as 0)'
        );

        local $@ = undef;
        eval { timegm( 0, 0, 0, 29, 1, 1904 ) };
        is( $@, q{}, 'no error with leap day of 1904' );

        local $@ = undef;
        eval { timegm( 0, 0, 0, 29, 1, 4 ) };
        is(
            $@, q{},
            'no error with leap day of 2004 (year passed as 4)'
        );

        local $@ = undef;
        eval { timegm( 0, 0, 0, 29, 1, 96 ) };
        is(
            $@, q{},
            'no error with leap day of 1996 (year passed as 96)'
        );
    },
);

subtest(
    'Large epoch values',
    sub {
        plan skip_all => 'These tests require support for large epoch values'
            unless $large_epoch_ok;

        is(
            timegm( 8, 14, 3, 19, 0, 2038 ), 2**31,
            'can call timegm for 2**31 epoch seconds'
        );
        is(
            timegm( 16, 28, 6, 7, 1, 2106 ), 2**32,
            'can call timegm for 2**32 epoch seconds (on a 64-bit system)'
        );
        is(
            timegm( 16, 36, 0, 20, 1, 36812 ), 2**40,
            'can call timegm for 2**40 epoch seconds (on a 64-bit system)'
        );
    },
);

subtest(
    '2-digit years',
    sub {
        my $current_year = ( localtime() )[5];
        my $pre_break    = ( $current_year + 49 ) - 100;
        my $break        = ( $current_year + 50 ) - 100;
        my $post_break   = ( $current_year + 51 ) - 100;

        subtest(
            'legacy year munging',
            sub {
                plan skip_all => 'Requires support for an large epoch values'
                    unless $large_epoch_ok;

                is(
                    (
                        (
                            localtime(
                                timelocal( 0, 0, 0, 1, 1, $pre_break )
                            )
                        )[5]
                    ),
                    $pre_break + 100,
                    "year $pre_break is treated as next century",
                );
                is(
                    (
                        ( localtime( timelocal( 0, 0, 0, 1, 1, $break ) ) )[5]
                    ),
                    $break + 100,
                    "year $break is treated as next century",
                );
                is(
                    (
                        (
                            localtime(
                                timelocal( 0, 0, 0, 1, 1, $post_break )
                            )
                        )[5]
                    ),
                    $post_break,
                    "year $post_break is treated as current century",
                );
            }
        );

        subtest(
            'modern',
            sub {
                plan skip_all =>
                    'Requires negative epoch support and large epoch support'
                    unless $neg_epoch_ok && $large_epoch_ok;

                is(
                    (
                        (
                            localtime(
                                timelocal_modern( 0, 0, 0, 1, 1, $pre_break )
                            )
                        )[5]
                    ) + 1900,
                    $pre_break,
                    "year $pre_break is treated as year $pre_break",
                );
                is(
                    (
                        (
                            localtime(
                                timelocal_modern( 0, 0, 0, 1, 1, $break )
                            )
                        )[5]
                    ) + 1900,
                    $break,
                    "year $break is treated as year $break",
                );
                is(
                    (
                        (
                            localtime(
                                timelocal_modern(
                                    0, 0, 0, 1, 1, $post_break
                                )
                            )
                        )[5]
                    ) + 1900,
                    $post_break,
                    "year $post_break is treated as year $post_break",
                );
            },
        );
    },
);

done_testing();
