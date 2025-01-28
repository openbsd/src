#!perl -w

BEGIN {
    unshift @INC, "../../t";
    require 'loc_tools.pl';
}

use strict;

use Config;
use POSIX;
use Test::More tests => 30;

# For the first go to UTC to avoid DST issues around the world when testing.  SUS3 says that
# null should get you UTC, but some environments want the explicit names.
# Those with a working tzset() should be able to use the TZ below.
$ENV{TZ} = "EST5EDT";

SKIP: {
    # It looks like POSIX.xs claims that only VMS and Mac OS traditional
    # don't have tzset().  Win32 works to call the function, but it doesn't
    # actually do anything.  Cygwin works in some places, but not others.  The
    # other Win32's below are guesses.
    skip "No tzset()", 1
       if $^O eq "VMS" || $^O eq "cygwin" ||
          $^O eq "MSWin32" || $^O eq "interix";
    tzset();
    SKIP: {
        my @tzname = tzname();

        # See extensive discussion in GH #22062.
        skip 1 if $tzname[1] ne "EDT";
        is(strftime("%Y-%m-%d %H:%M:%S", 0, 30, 2, 10, 2, 124, 0, 0, 0),
                    "2024-03-10 02:30:00",
                    "strftime() doesnt pay attention to dst");
    }
}

# go to UTC to avoid DST issues around the world when testing.  SUS3 says that
# null should get you UTC, but some environments want the explicit names.
# Those with a working tzset() should be able to use the TZ below.
$ENV{TZ} = "UTC0UTC";

SKIP: {
    skip "No tzset()", 2
       if $^O eq "VMS" || $^O eq "cygwin" ||
          $^O eq "MSWin32" || $^O eq "interix";
    tzset();
    my @tzname = tzname();
    like($tzname[0], qr/(GMT|UTC)/i, "tzset() to GMT/UTC");
    SKIP: {
        skip "Mac OS X/Darwin doesn't handle this", 1 if $^O =~ /darwin/i;
        like($tzname[1], qr/(GMT|UTC)/i, "The whole year?");
    }
}

if ($^O eq "hpux" && $Config{osvers} >= 11.3) {
    # HP does not support UTC0UTC and/or GMT0GMT, as they state that this is
    # legal syntax but as it has no DST rule, it cannot be used. That is the
    # conclusion of bug
    # QXCR1000896916: Some timezone valuesfailing on 11.31 that work on 11.23
    $ENV{TZ} = "UTC";
}

# asctime and ctime...Let's stay below INT_MAX for 32-bits and
# positive for some picky systems.

is(asctime(CORE::localtime(0)), ctime(0), "asctime() and ctime() at zero");
is(asctime(POSIX::localtime(0)), ctime(0), "asctime() and ctime() at zero");
is(asctime(CORE::localtime(12345678)), ctime(12345678),
   "asctime() and ctime() at 12345678");
is(asctime(POSIX::localtime(12345678)), ctime(12345678),
   "asctime() and ctime() at 12345678");

my $illegal_format = "%!";

# An illegal format could result in an empty result, but many platforms just
# pass it through, or strip off the '%'
sub munge_illegal_format_result($) {
    my $result = shift;
    $result = "" if $result eq $illegal_format || $result eq '!';
    return $result;
}

my $jan_16 = 15 * 86400;

is(munge_illegal_format_result(strftime($illegal_format,
                                        CORE::localtime($jan_16))),
   "", "strftime returns appropriate result for an illegal format");

# Careful!  strftime() is locale sensitive.  Let's take care of that
my $orig_time_loc = 'C';

my $LC_TIME_enabled = locales_enabled('LC_TIME');
if ($LC_TIME_enabled) {
    $orig_time_loc = setlocale(LC_TIME) || die "Cannot get time locale information:  $!";
    setlocale(LC_TIME, "C") || die "Cannot setlocale() to C:  $!";
}

my $ctime_format = "%a %b %d %H:%M:%S %Y\n";
is(ctime($jan_16), strftime($ctime_format, CORE::localtime($jan_16)),
        "get ctime() equal to strftime()");
is(ctime($jan_16), strftime($ctime_format, POSIX::localtime($jan_16)),
        "get localtime() equal to strftime()");

my $ss = chr 223;
unlike($ss, qr/\w/, 'Not internally UTF-8 encoded');
is(ord strftime($ss, CORE::localtime), 223,
   'Format string has correct character');
is(ord strftime($ss, POSIX::localtime(time)),
   223, 'Format string has correct character');
unlike($ss, qr/\w/, 'Still not internally UTF-8 encoded');

my $zh_format = "%Y\x{5e74}%m\x{6708}%d\x{65e5}";
my $zh_expected_result = "1970\x{5e74}01\x{6708}16\x{65e5}";
isnt(strftime($zh_format, CORE::gmtime($jan_16)),
              $zh_expected_result,
           "strftime() UTF-8 format doesn't return UTF-8 in non-UTF-8 locale");

my $utf8_locale = find_utf8_ctype_locale();
SKIP: {
    my $has_time_utf8_locale = ($LC_TIME_enabled && defined $utf8_locale);
    if ($has_time_utf8_locale) {
        my $time_utf8_locale = setlocale(LC_TIME, $utf8_locale);

        # Some platforms don't allow LC_TIME to be changed to a UTF-8 locale,
        # even if we have found one whose LC_CTYPE can be.  The next two tests
        # are invalid on such platforms.  Check for that.  (Examples include
        # OpenBSD, and Alpine Linux without the add-on locales package
        # installed.)
        if (   ! defined $time_utf8_locale
            || ! is_locale_utf8($time_utf8_locale))
        {
            $has_time_utf8_locale = 0;
        }
    }

    skip "No LC_TIME UTF-8 locale", 2 unless $has_time_utf8_locale;

    # By setting LC_TIME only, we verify that the code properly handles the
    # case where that and LC_CTYPE differ
    is(strftime($zh_format, CORE::gmtime($jan_16)),
                $zh_expected_result,
                "strftime() can handle a UTF-8 format;  LC_CTYPE != LCTIME");
    is(strftime($zh_format, POSIX::gmtime($jan_16)),
                $zh_expected_result,
                "Same, but uses POSIX::gmtime; previous test used CORE::");
    setlocale(LC_TIME, "C") || die "Cannot setlocale() to C: $!";
}

my $non_C_locale = $utf8_locale;
if (! defined $non_C_locale) {
    my @locales = find_locales(LC_CTYPE);
    while (@locales) {
        if ($locales[0] ne "C") {
            $non_C_locale = $locales[0];
            last;
        }

        shift @locales;
    }
}

SKIP: {
    skip "No non-C locale", 4 if ! locales_enabled(LC_CTYPE)
                              || ! defined $non_C_locale;
    my $orig_ctype_locale = setlocale(LC_CTYPE)
                            || die "Cannot get ctype locale information:  $!";
    setlocale(LC_CTYPE, $non_C_locale)
                    || die "Cannot setlocale(LC_CTYPE) to $non_C_locale:  $!";

    is(ctime($jan_16), strftime($ctime_format, CORE::localtime($jan_16)),
       "Repeat of ctime() equal to strftime()");
    is(setlocale(LC_CTYPE), $non_C_locale, "strftime restores LC_CTYPE");

    is(munge_illegal_format_result(strftime($illegal_format,
                                            CORE::localtime($jan_16))),
       "", "strftime returns appropriate result for an illegal format");
    is(setlocale(LC_CTYPE), $non_C_locale,
       "strftime restores LC_CTYPE even on failure");

    setlocale(LC_CTYPE, $orig_ctype_locale)
                          || die "Cannot setlocale(LC_CTYPE) back to orig: $!";
}

if ($LC_TIME_enabled) {
    setlocale(LC_TIME, $orig_time_loc)
                            || die "Cannot setlocale(LC_TIME) back to orig: $!";
}

# clock() seems to have different definitions of what it does between POSIX
# and BSD.  Cygwin, Win32, and Linux lean the BSD way.  So, the tests just
# check the basics.
like(clock(), qr/\d*/, "clock() returns a numeric value");
cmp_ok(clock(), '>=', 0, "...and it returns something >= 0");

SKIP: {
    skip "No difftime()", 1 if $Config{d_difftime} ne 'define';
    is(difftime(2, 1), 1, "difftime()");
}

SKIP: {
    skip "No mktime()", 2 if $Config{d_mktime} ne 'define';
    my $time = time();
    is(mktime(CORE::localtime($time)), $time, "mktime()");
    is(mktime(POSIX::localtime($time)), $time, "mktime()");
}

{
    # GH #22498
    is(strftime(42, CORE::localtime), '42', "strftime() works if format is a number");
    my $obj = bless {}, 'Some::Random::Class';
    is(strftime($obj, CORE::localtime), "$obj", "strftime() works if format is an object");
    my $warnings = '';
    local $SIG{__WARN__} = sub { $warnings .= $_[0] };
    is(strftime(undef, CORE::localtime), '', "strftime() works if format is undef");
    like($warnings, qr/^Use of uninitialized value in subroutine entry /, "strftime(undef, ...) produces expected warning");
}
