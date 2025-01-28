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

# The values a C locale should return
my %want = (    RADIXCHAR => qr/ ^ \. $ /x,
                THOUSEP	  => qr/ ^$ /x,

                # Can be empty; otherwise first character must be one of
                # these.  In the C locale, there is nothing after the first
                # character.
                CRNCYSTR  => qr/ ^ [+-.]? $ /x,

                _NL_ADDRESS_COUNTRY_NUM => qr/^ 0 $/x,
                _NL_IDENTIFICATION_TERRITORY => qr/ ^ ISO $/x,
                _NL_MEASUREMENT_MEASUREMENT => qr/ ^ [01] $/x,
                _NL_PAPER_HEIGHT => qr/^ \d+ $/x,
                _NL_NAME_NAME_GEN => qr/ .* /x,
                _NL_TELEPHONE_INT_SELECT => qr/ .* /x,
           );

# Abbreviated and full are swapped in many locales in early netbsd.  Skip
# them.
if (   $Config{osname} !~ / netbsd /ix
    || $Config{osvers} !~ / ^ [1-6] \. /x)
{
    $want{ABDAY_1} = qr/ ^ Sun $ /x;
    $want{DAY_1}   = qr/ ^ Sunday $ /x;
    $want{ABMON_1} = qr/ ^ Jan $ /x;
    $want{MON_1}   = qr/ ^ January $ /x;
}

sub disp_str ($) {
    my $string = shift;

    # Displays the string unambiguously.  ASCII printables are always output
    # as-is, though perhaps separated by blanks from other characters.  If
    # entirely printable ASCII, just returns the string.  Otherwise if valid
    # UTF-8 it uses the character names for non-printable-ASCII.  Otherwise it
    # outputs hex for each non-ASCII-printable byte.

    return $string if $string =~ / ^ [[:print:]]* $/xa;

    my $result = "";
    my $prev_was_punct = 1; # Beginning is considered punct
    if (utf8::valid($string) && utf8::is_utf8($string)) {
        use charnames ();
        foreach my $char (split "", $string) {

            # Keep punctuation adjacent to other characters; otherwise
            # separate them with a blank
            if ($char =~ /[[:punct:]]/a) {
                $result .= $char;
                $prev_was_punct = 1;
            }
            elsif ($char =~ /[[:print:]]/a) {
                $result .= "  " unless $prev_was_punct;
                $result .= $char;
                $prev_was_punct = 0;
            }
            else {
                $result .= "  " unless $prev_was_punct;
                my $name = charnames::viacode(ord $char);
                $result .= (defined $name) ? $name : ':unknown:';
                $prev_was_punct = 0;
            }
        }
    }
    else {
        use bytes;
        foreach my $char (split "", $string) {
            if ($char =~ /[[:punct:]]/a) {
                $result .= $char;
                $prev_was_punct = 1;
            }
            elsif ($char =~ /[[:print:]]/a) {
                $result .= " " unless $prev_was_punct;
                $result .= $char;
                $prev_was_punct = 0;
            }
            else {
                $result .= " " unless $prev_was_punct;
                $result .= sprintf("%02X", ord $char);
                $prev_was_punct = 0;
            }
        }
    }

    return $result;
}

sub check_utf8_validity($$$) {

    # Looks for a definitive result for testing perl code on UTF-8 locales.
    # Returns 1 if definitive (one way or another).
    # Returns 0 if the input is all ASCII.
    # Returns -1 if it looks to be a system error

    my ($string, $item, $locale) = @_;
    my $msg_details = "The name for '$item' in $locale";

    return 0 unless $string =~ /\P{ASCII}/;

    if (utf8::is_utf8($string)) {
        if (utf8::valid($string )) {
            pass("$msg_details is a UTF8 string.  Got:\n" . disp_str($string));
            return 1;
        }

        # Here, marked as UTF-8, but is malformed, so shouldn't have been
        # marked thus
        fail("$msg_details is marked as UTF8 but is malformed.  Got:\n"
           . disp_str($string));
        return 1;
    }

    # Here, not marked as UTF-8.  Since this is a UTF-8 locale, and contains
    # non-ASCII, something is wrong.  It may be us, or it may be libc.  Use
    # decode to see if the bytes form legal UTF-8.  If they did, it means
    # perl wrongly returned the string as not UTF-8.
    my $copy = $string;
    my $is_valid_utf8;
    {
        use bytes;
        $is_valid_utf8 = utf8::decode($copy);
    }

    if ($is_valid_utf8) {
        fail("$msg_details should have been marked as a UTF8 string.  Got:\n"
           . disp_str($string));
        return 1;
    }

    # Here, the string returned wasn't marked as UTF-8 and isn't valid UTF-8.
    # This means perl did its job and kept malformed text from being marked
    # UTF-8.  And it means a system bug since the locale was UTF-8.
    return -1;
}

my @want = sort keys %want;
my @illegal_utf8;

my %extra_items = (
                    _NL_ADDRESS_POSTAL_FMT => 'LC_ADDRESS',
                    _NL_ADDRESS_COUNTRY_NAME => 'LC_ADDRESS',
                    _NL_ADDRESS_COUNTRY_POST => 'LC_ADDRESS',
                    _NL_ADDRESS_COUNTRY_AB2 => 'LC_ADDRESS',
                    _NL_ADDRESS_COUNTRY_AB3 => 'LC_ADDRESS',
                    _NL_ADDRESS_COUNTRY_CAR => 'LC_ADDRESS',
                    _NL_ADDRESS_COUNTRY_NUM => 'LC_ADDRESS',
                    _NL_ADDRESS_COUNTRY_ISBN => 'LC_ADDRESS',
                    _NL_ADDRESS_LANG_NAME => 'LC_ADDRESS',
                    _NL_ADDRESS_LANG_AB => 'LC_ADDRESS',
                    _NL_ADDRESS_LANG_TERM => 'LC_ADDRESS',
                    _NL_ADDRESS_LANG_LIB => 'LC_ADDRESS',
                    _NL_IDENTIFICATION_TITLE => 'LC_IDENTIFICATION',
                    _NL_IDENTIFICATION_SOURCE => 'LC_IDENTIFICATION',
                    _NL_IDENTIFICATION_ADDRESS => 'LC_IDENTIFICATION',
                    _NL_IDENTIFICATION_CONTACT => 'LC_IDENTIFICATION',
                    _NL_IDENTIFICATION_EMAIL => 'LC_IDENTIFICATION',
                    _NL_IDENTIFICATION_TEL => 'LC_IDENTIFICATION',
                    _NL_IDENTIFICATION_FAX => 'LC_IDENTIFICATION',
                    _NL_IDENTIFICATION_LANGUAGE => 'LC_IDENTIFICATION',
                    _NL_IDENTIFICATION_TERRITORY => 'LC_IDENTIFICATION',
                    _NL_IDENTIFICATION_AUDIENCE => 'LC_IDENTIFICATION',
                    _NL_IDENTIFICATION_APPLICATION => 'LC_IDENTIFICATION',
                    _NL_IDENTIFICATION_ABBREVIATION => 'LC_IDENTIFICATION',
                    _NL_IDENTIFICATION_REVISION => 'LC_IDENTIFICATION',
                    _NL_IDENTIFICATION_DATE => 'LC_IDENTIFICATION',
                    _NL_IDENTIFICATION_CATEGORY => 'LC_IDENTIFICATION',
                    _NL_MEASUREMENT_MEASUREMENT => 'LC_MEASUREMENT',
                    _NL_NAME_NAME_FMT => 'LC_NAME',
                    _NL_NAME_NAME_GEN => 'LC_NAME',
                    _NL_NAME_NAME_MR => 'LC_NAME',
                    _NL_NAME_NAME_MRS => 'LC_NAME',
                    _NL_NAME_NAME_MISS => 'LC_NAME',
                    _NL_NAME_NAME_MS => 'LC_NAME',
                    _NL_PAPER_HEIGHT => 'LC_PAPER',
                    _NL_PAPER_WIDTH => 'LC_PAPER',
                    _NL_TELEPHONE_TEL_INT_FMT => 'LC_TELEPHONE',
                    _NL_TELEPHONE_TEL_DOM_FMT => 'LC_TELEPHONE',
                    _NL_TELEPHONE_INT_SELECT => 'LC_TELEPHONE',
                    _NL_TELEPHONE_INT_PREFIX => 'LC_TELEPHONE',
                );

use_ok('I18N::Langinfo', 'langinfo', @constants, 'CRNCYSTR',
                          keys %extra_items);

use POSIX;

if (locales_enabled('LC_ALL')) {
    setlocale(LC_ALL, "C");
}
else { # If no LC_ALL, make sure the categories used in Langinfo are in C
    setlocale(LC_CTYPE, "C")          if locales_enabled('LC_CTYPE');
    setlocale(LC_MONETARY, "C")       if locales_enabled('LC_MONETARY');
    setlocale(LC_NUMERIC, "C")        if locales_enabled('LC_NUMERIC');
    setlocale(LC_TIME, "C")           if locales_enabled('LC_TIME');
    setlocale(LC_ADDRESS, "C")        if locales_enabled('LC_ADDRESS');
    setlocale(LC_IDENTIFICATION, "C") if locales_enabled('LC_IDENTIFICATION');
    setlocale(LC_MEASUREMENT, "C")    if locales_enabled('LC_MEASUREMENT');
    setlocale(LC_NAME, "C")           if locales_enabled('LC_NAME');
    setlocale(LC_PAPER, "C")          if locales_enabled('LC_PAPER');
    setlocale(LC_TELEPHONE, "C")      if locales_enabled('LC_TELEPHONE');
}

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
        like (langinfo(&$try), $want{$try}, "$try => '$want{$try}'");
    }
}

my $comma_locale;
for my $locale (find_locales( 'LC_NUMERIC' )) {
    use POSIX;
    use locale;
    setlocale(LC_NUMERIC, $locale) or next;
    my $in = 4.2; # avoid any constant folding bugs
    my $s = sprintf("%g", $in);
    if ($s eq "4,2")  {
        $comma_locale = $locale;
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

    my %time_locales;
    map { $time_locales{$_} = 1 } find_locales("LC_TIME");
    my %monetary_locales;
    map { $monetary_locales{$_} = 1 } find_locales("LC_MONETARY");

    foreach my $utf8_locale (find_utf8_ctype_locales()) {
        if ($time_locales{$utf8_locale} && ! $found_time) {
            setlocale(&LC_TIME, $utf8_locale);
            foreach my $time_item (@times) {
                my $eval_string = "langinfo(&$time_item)";
                my $time_name = eval $eval_string;
                if ($@) {
                    fail("'$eval_string' failed: $@");

                    # If this or the next two tests fail, any other items or
                    # locales will likely fail too, so skip testing them.
                    $found_time = 1;
                    last;
                }
                if (! defined $time_name) {
                    fail("'$eval_string' returned undef");
                    $found_time = 1;
                    last;
                }
                if ($time_name eq "") {
                    fail("'$eval_string' returned an empty name");
                    $found_time = 1;
                    last;
                }

                my $ret = check_utf8_validity($time_name, $time_item, $utf8_locale);
                if ($ret > 0) {
                    $found_time = 1;
                    last;
                }

                if ($ret < 0) { # < 0 means a system error
                    push @illegal_utf8, "$utf8_locale: $time_item:"
                                     .  disp_str($time_name);
                }
            }
        }

        if ($monetary_locales{$utf8_locale} && ! $found_monetary) {
            setlocale(&LC_MONETARY, $utf8_locale);
            my $eval_string = "langinfo(&CRNCYSTR)";
            my $symbol = eval $eval_string;
            if ($@) {
                fail("'$eval_string' failed: $@");
                $found_monetary = 1;
                next;
            }
            if (! defined $symbol) {
                fail("'$eval_string' returned undef");
                next;
            }

            my $ret = check_utf8_validity($symbol, 'CRNCY', $utf8_locale);
            if ($ret > 0) {
                $found_monetary = 1;
            }
            elsif ($ret < 0) { # < 0 means a system error
                push @illegal_utf8, "$utf8_locale: CRNCY:"
                                 .  disp_str($symbol);
            }
        }

        last if $found_monetary && $found_time;
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

if (@illegal_utf8) {
    diag join "\n", "The following are illegal UTF-8", @illegal_utf8;
}

done_testing();
