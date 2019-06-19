#!perl -T
use strict;
use Config;
use Test::More;
require "../../t/loc_tools.pl";

plan skip_all => "I18N::Langinfo or POSIX unavailable" 
    if $Config{'extensions'} !~ m!\bI18N/Langinfo\b!;

my @times  = qw( MON_1 MON_2 MON_3 MON_4 MON_5 MON_6 MON_7
                 MON_8 MON_9 MON_10 MON_11 MON_12
                 DAY_1 DAY_2 DAY_3 DAY_4 DAY_5 DAY_6 DAY_7);
my @constants = qw(ABDAY_1 DAY_1 ABMON_1 RADIXCHAR AM_STR THOUSEP D_T_FMT
                   D_FMT T_FMT);
push @constants, @times;

my %want =
    (
        RADIXCHAR	=> ".",
        THOUSEP	=> "",
     );

# Abbreviated and full are swapped in many locales in early netbsd
if (   $Config{osname} !~ / netbsd /ix
    || $Config{osvers} !~ / ^ [1-6] \. /x)
{
    $want{ABDAY_1} = "Sun";
    $want{DAY_1}   = "Sunday";
    $want{ABMON_1} = "Jan";
    $want{MON_1}   = "January";
}

my @want = sort keys %want;

plan tests => 1 + 3 * @constants + keys(@want) + 1 + 2;

use_ok('I18N::Langinfo', 'langinfo', @constants, 'CRNCYSTR');

use POSIX;
setlocale(LC_ALL, "C");

for my $constant (@constants) {
    SKIP: {
        my $string = eval { langinfo(eval "$constant()") };
        is( $@, '', "calling langinfo() with $constant" );
        skip "returned string was empty, skipping next two tests", 2 unless $string;
        ok( defined $string, "checking if the returned string is defined" );
        cmp_ok( length($string), '>=', 1, "checking if the returned string has a positive length" );
    }
}

for my $i (1..@want) {
    my $try = $want[$i-1];
    eval { I18N::Langinfo->import($try) };
    SKIP: {
        skip "$try not defined", 1, if $@;
        no strict 'refs';
        is (langinfo(&$try), $want{$try}, "$try => '$want{$try}'");
    }
}

my $comma_locale;
for (find_locales( [ 'LC_NUMERIC' ] )) {
    use POSIX;
    use locale;
    setlocale(LC_NUMERIC, $_) or next;
    my $in = 4.2; # avoid any constant folding bugs
    my $s = sprintf("%g", $in);
    if ($s eq "4,2")  {
        $comma_locale = $_;
        last;
    }
}

SKIP: {
    skip "Couldn't find a locale with a comma decimal pt", 1
                                                        unless $comma_locale;

    no strict 'refs';
    is (langinfo(&RADIXCHAR), ",",
        "Returns ',' for decimal pt for locale '$comma_locale'");
}

SKIP: {

    my $found_time = 0;
    my $found_monetary = 0;
    my @locales = find_locales( [ 'LC_TIME', 'LC_CTYPE', 'LC_MONETARY' ]);

    while (defined (my $utf8_locale = find_utf8_ctype_locale(\@locales))) {
        if (! $found_time) {
            setlocale(&LC_TIME, $utf8_locale);
            foreach my $time_item (@times) {
                my $eval_string = "langinfo(&$time_item)";
                my $time_name = eval $eval_string;
                if ($@) {
                    fail("'$eval_string' failed: $@");
                    last SKIP;
                }
                if (! defined $time_name) {
                    fail("'$eval_string' returned undef");
                    last SKIP;
                }
                if ($time_name eq "") {
                    fail("'$eval_string' returned an empty name");
                    last SKIP;
                }

                if ($time_name =~ /\P{ASCII}/) {
                    ok(utf8::is_utf8($time_name), "The name for '$time_item' in $utf8_locale is a UTF8 string");
                    $found_time = 1;
                    last;
                }
            }
        }

        if (! $found_monetary) {
            setlocale(&LC_MONETARY, $utf8_locale);
            my $eval_string = "langinfo(&CRNCYSTR)";
            my $symbol = eval $eval_string;
            if ($@) {
                fail("'$eval_string' failed: $@");
                last SKIP;
            }
            if (! defined $symbol) {
                fail("'$eval_string' returned undef");
                last SKIP;
            }
            if ($symbol =~ /\P{ASCII}/) {
                ok(utf8::is_utf8($symbol), "The name for 'CRNCYSTR' in $utf8_locale is a UTF8 string");
                $found_monetary = 1;
            }
        }

        last if $found_monetary && $found_time;

        # Remove this locale from the list, and loop to find another utf8
        # locale
        @locales = grep { $_ ne $utf8_locale } @locales;
    }

    if ($found_time + $found_monetary < 2) {
        my $message = "";
        $message .= "time name" unless $found_time;
        if (! $found_monetary) {
            $message .= " nor" if $message;
            "monetary name";
        }
        skip("Couldn't find a locale with a non-ascii $message", 2 - $found_time - $found_monetary);
    }
}
