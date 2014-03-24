#!./perl

use strict;
use warnings;

use Config;
use Test::More 0.88;
use Time::Local;

# Set up time values to test
my @time =
  (
   #year,mon,day,hour,min,sec
   [1970,  1,  2, 00, 00, 00],
   [1980,  2, 28, 12, 00, 00],
   [1980,  2, 29, 12, 00, 00],
   [1999, 12, 31, 23, 59, 59],
   [2000,  1,  1, 00, 00, 00],
   [2010, 10, 12, 14, 13, 12],
   # leap day
   [2020,  2, 29, 12, 59, 59],
   [2030,  7,  4, 17, 07, 06],

# The following test fails on a surprising number of systems
# so it is commented out. The end of the Epoch for a 32-bit signed
# implementation of time_t should be Jan 19, 2038  03:14:07 UTC.
#  [2038,  1, 17, 23, 59, 59],     # last full day in any tz

   [2010, 10, 12, 14, 13, 12.1],
   [2010, 10, 12, 14, 13, 59.1],
  );

# more than 2**31 time_t - requires a 64bit safe localtime/gmtime
push @time, [2258,  8, 11,  1, 49, 17]
    if $] >= 5.012000;

my @bad_time =
    (
     # month too large
     [1995, 13, 01, 01, 01, 01],
     # day too large
     [1995, 02, 30, 01, 01, 01],
     # hour too large
     [1995, 02, 10, 25, 01, 01],
     # minute too large
     [1995, 02, 10, 01, 60, 01],
     # second too large
     [1995, 02, 10, 01, 01, 60],
    );

my @neg_time =
    (
     # test negative epochs for systems that handle it
     [ 1969, 12, 31, 16, 59, 59 ],
     [ 1950, 04, 12, 9, 30, 31 ],
    );

# Leap year tests
my @years =
    (
     [ 1900 => 0 ],
     [ 1947 => 0 ],
     [ 1996 => 1 ],
     [ 2000 => 1 ],
     [ 2100 => 0 ],
    );

# Use 3 days before the start of the epoch because with Borland on
# Win32 it will work for -3600 _if_ your time zone is +01:00 (or
# greater).
my $neg_epoch_ok = defined ((localtime(-259200))[0]) ? 1 : 0;

# use vmsish 'time' makes for oddness around the Unix epoch
if ($^O eq 'VMS') {
    $time[0][2]++;
    $neg_epoch_ok = 0; # time_t is unsigned
}

my $epoch_is_64 = eval { $Config{ivsize} == 8 && ( gmtime 2**40 )[5] == 34912 };

for (@time, @neg_time) {
    my($year, $mon, $mday, $hour, $min, $sec) = @$_;
    $year -= 1900;
    $mon--;

    SKIP: {
        skip '1970 test on VOS fails.', 12
            if $^O eq 'vos' && $year == 70;
        skip 'this platform does not support negative epochs.', 12
            if $year < 70 && ! $neg_epoch_ok;

        # Test timelocal()
        {
            my $year_in = $year < 70 ? $year + 1900 : $year;
            my $time = timelocal($sec,$min,$hour,$mday,$mon,$year_in);

            my($s,$m,$h,$D,$M,$Y) = localtime($time);

            is($s, int($sec), "timelocal second for @$_");
            is($m, $min, "timelocal minute for @$_");
            is($h, $hour, "timelocal hour for @$_");
            is($D, $mday, "timelocal day for @$_");
            is($M, $mon, "timelocal month for @$_");
            is($Y, $year, "timelocal year for @$_");
        }


        # Test timegm()
        {
            my $year_in = $year < 70 ? $year + 1900 : $year;
            my $time = timegm($sec,$min,$hour,$mday,$mon,$year_in);

            my($s,$m,$h,$D,$M,$Y) = gmtime($time);

            is($s, int($sec), "timegm second for @$_");
            is($m, $min, "timegm minute for @$_");
            is($h, $hour, "timegm hour for @$_");
            is($D, $mday, "timegm day for @$_");
            is($M, $mon, "timegm month for @$_");
            is($Y, $year, "timegm year for @$_");
        }
    }
}


for (@bad_time) {
    my($year, $mon, $mday, $hour, $min, $sec) = @$_;
    $year -= 1900;
    $mon--;

    eval { timegm($sec,$min,$hour,$mday,$mon,$year) };

    like($@, qr/.*out of range.*/, 'invalid time caused an error');
}

{
    is(timelocal(0,0,1,1,0,90) - timelocal(0,0,0,1,0,90), 3600,
       'one hour difference between two calls to timelocal');

    is(timelocal(1,2,3,1,0,100) - timelocal(1,2,3,31,11,99), 24 * 3600,
       'one day difference between two calls to timelocal');

    # Diff beween Jan 1, 1980 and Mar 1, 1980 = (31 + 29 = 60 days)
    is(timegm(0,0,0, 1, 2, 80) - timegm(0,0,0, 1, 0, 80), 60 * 24 * 3600,
       '60 day difference between two calls to timegm');
}

# bugid #19393
# At a DST transition, the clock skips forward, eg from 01:59:59 to
# 03:00:00. In this case, 02:00:00 is an invalid time, and should be
# treated like 03:00:00 rather than 01:00:00 - negative zone offsets used
# to do the latter
{
    my $hour = (localtime(timelocal(0, 0, 2, 7, 3, 102)))[2];
    # testers in US/Pacific should get 3,
    # other testers should get 2
    ok($hour == 2 || $hour == 3, 'hour should be 2 or 3');
}

for my $p (@years) {
    my ( $year, $is_leap_year ) = @$p;

    my $string = $is_leap_year ? 'is' : 'is not';
    is( Time::Local::_is_leap_year($year), $is_leap_year,
        "$year $string a leap year" );
}

SKIP:
{
    skip 'this platform does not support negative epochs.', 6
        unless $neg_epoch_ok;

    eval { timegm(0,0,0,29,1,1900) };
    like($@, qr/Day '29' out of range 1\.\.28/,
         'does not accept leap day in 1900');

    eval { timegm(0,0,0,29,1,200) };
    like($@, qr/Day '29' out of range 1\.\.28/,
         'does not accept leap day in 2100 (year passed as 200)');

    eval { timegm(0,0,0,29,1,0) };
    is($@, '', 'no error with leap day of 2000 (year passed as 0)');

    eval { timegm(0,0,0,29,1,1904) };
    is($@, '', 'no error with leap day of 1904');

    eval { timegm(0,0,0,29,1,4) };
    is($@, '', 'no error with leap day of 2004 (year passed as 4)');

    eval { timegm(0,0,0,29,1,96) };
    is($@, '', 'no error with leap day of 1996 (year passed as 96)');
}

SKIP:
{
    skip 'These tests require a system with 64-bit time_t.', 3
        unless $epoch_is_64;

    is( timegm( 8, 14, 3, 19, 0, ( 1900 + 138 ) ), 2**31,
        'can call timegm for 2**31 epoch seconds' );
    is( timegm( 16, 28, 6, 7, 1, ( 1900 + 206 ) ), 2**32,
        'can call timegm for 2**32 epoch seconds (on a 64-bit system)' );
    is( timegm( 16, 36, 0, 20, 1, ( 34912 + 1900 ) ), 2**40,
        'can call timegm for 2**40 epoch seconds (on a 64-bit system)' );
}

SKIP:
{
    skip 'These tests only run for the package maintainer.', 8
        unless $ENV{MAINTAINER};

    require POSIX;

    local $ENV{TZ} = 'Europe/Vienna';
    POSIX::tzset();

    # 2001-10-28 02:30:00 - could be either summer or standard time,
    # prefer earlier of the two, in this case summer
    my $time = timelocal(0, 30, 2, 28, 9, 101);
    is($time, 1004229000,
       'timelocal prefers earlier epoch in the presence of a DST change');

    local $ENV{TZ} = 'America/Chicago';
    POSIX::tzset();

    # Same local time in America/Chicago.  There is a transition here
    # as well.
    $time = timelocal(0, 30, 1, 28, 9, 101);
    is($time, 1004250600,
       'timelocal prefers earlier epoch in the presence of a DST change');

    $time = timelocal(0, 30, 2, 1, 3, 101);
    is($time, 986113800,
       'timelocal for non-existent time gives you the time one hour later');

    local $ENV{TZ} = 'Australia/Sydney';
    POSIX::tzset();
    # 2001-03-25 02:30:00 in Australia/Sydney.  This is the transition
    # _to_ summer time.  The southern hemisphere transitions are
    # opposite those of the northern.
    $time = timelocal(0, 30, 2, 25, 2, 101);
    is($time, 985447800,
       'timelocal prefers earlier epoch in the presence of a DST change');

    $time = timelocal(0, 30, 2, 28, 9, 101);
    is($time, 1004200200,
       'timelocal for non-existent time gives you the time one hour later');

    local $ENV{TZ} = 'Europe/London';
    POSIX::tzset();
    $time = timelocal( localtime(1111917720) );
    is($time, 1111917720,
       'timelocal for round trip bug on date of DST change for Europe/London');

    # There is no 1:00 AM on this date, as it leaps forward to
    # 2:00 on the DST change - this should return 2:00 per the
    # docs.
    is( ( localtime( timelocal( 0, 0, 1, 27, 2, 2005 ) ) )[2], 2,
        'hour is 2 when given 1:00 AM on Europe/London date change' );

    is( ( localtime( timelocal( 0, 0, 2, 27, 2, 2005 ) ) )[2], 2,
        'hour is 2 when given 2:00 AM on Europe/London date change' );
}

done_testing();
