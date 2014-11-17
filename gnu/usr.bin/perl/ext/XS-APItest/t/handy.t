#!perl -w

BEGIN {
    require 'loc_tools.pl';   # Contains find_utf8_ctype_locale()
}

use strict;
use Test::More;
use Config;

use XS::APItest;

use Unicode::UCD qw(prop_invlist prop_invmap);

sub truth($) {  # Converts values so is() works
    return (shift) ? 1 : 0;
}

my $locale;
my $utf8_locale;
if($Config{d_setlocale}) {
    require POSIX;
    $locale = POSIX::setlocale( &POSIX::LC_ALL, "C");
    if (defined $locale && $locale eq 'C') {
        BEGIN {
            if($Config{d_setlocale}) {
                require locale; import locale; # make \w work right in non-ASCII lands
            }
        }

        # Some locale implementations don't have the 128-255 characters all
        # mean nothing.  Skip the locale tests in that situation
        for my $i (128 .. 255) {
            if (chr($i) =~ /[[:print:]]/) {
                undef $locale;
                last;
            }
        }

        $utf8_locale = find_utf8_ctype_locale();
    }
}

my %properties = (
                   # name => Lookup-property name
                   alnum => 'Word',
                   wordchar => 'Word',
                   alphanumeric => 'Alnum',
                   alpha => 'Alpha',
                   ascii => 'ASCII',
                   blank => 'Blank',
                   cntrl => 'Control',
                   digit => 'Digit',
                   graph => 'Graph',
                   idfirst => '_Perl_IDStart',
                   idcont => '_Perl_IDCont',
                   lower => 'Lower',
                   print => 'Print',
                   psxspc => 'XPosixSpace',
                   punct => 'XPosixPunct',
                   quotemeta => '_Perl_Quotemeta',
                   space => 'XPerlSpace',
                   vertws => 'VertSpace',
                   upper => 'Upper',
                   xdigit => 'XDigit',
                );

my @warnings;
local $SIG{__WARN__} = sub { push @warnings, @_ };

use charnames ();
foreach my $name (sort keys %properties) {
    my $property = $properties{$name};
    my @invlist = prop_invlist($property, '_perl_core_internal_ok');
    if (! @invlist) {
        fail("No inversion list found for $property");
        next;
    }

    # Include all the Latin1 code points, plus 0x100.
    my @code_points = (0 .. 256);

    # Then include the next few boundaries above those from this property
    my $above_latins = 0;
    foreach my $range_start (@invlist) {
        next if $range_start < 257;
        push @code_points, $range_start - 1, $range_start;
        $above_latins++;
        last if $above_latins > 5;
    }

    # This makes sure we are using the Perl definition of idfirst and idcont,
    # and not the Unicode.  There are a few differences.
    push @code_points, ord "\N{ESTIMATED SYMBOL}" if $name =~ /^id(first|cont)/;
    if ($name eq "idcont") {    # And some that are continuation but not start
        push @code_points, ord("\N{GREEK ANO TELEIA}"),
                           ord("\N{COMBINING GRAVE ACCENT}");
    }

    # And finally one non-Unicode code point.
    push @code_points, 0x110000;    # Above Unicode, no prop should match
    no warnings 'non_unicode';

    for my $j (@code_points) {
        my $i = utf8::native_to_unicode($j);
        my $function = uc($name);

        my $matches = Unicode::UCD::search_invlist(\@invlist, $i);
        if (! defined $matches) {
            $matches = 0;
        }
        else {
            $matches = truth(! ($matches % 2));
        }

        my $ret;
        my $char_name = charnames::viacode($i) // "No name";
        my $display_name = sprintf "\\N{U+%02X, %s}", $i, $char_name;

        if ($name eq 'quotemeta') { # There is only one macro for this, and is
                                    # defined only for Latin1 range
            $ret = truth eval "test_is${function}($i)";
            if ($@) {
                fail $@;
            }
            else {
                my $truth = truth($matches && $i < 256);
                is ($ret, $truth, "is${function}( $display_name ) == $truth");
            }
            next;
        }

        # vertws is always all of Unicode; ALNUM_A and ALNUM_L1 are not
        # defined as they were added later, after WORDCHAR was created to be a
        # clearer synonym for ALNUM
        if ($name ne 'vertws') {
            if ($name ne 'alnum') {
                $ret = truth eval "test_is${function}_A($i)";
                if ($@) {
                    fail($@);
                }
                else {
                    my $truth = truth($matches && $i < 128);
                    is ($ret, $truth, "is${function}_A( $display_name ) == $truth");
                }
                $ret = truth eval "test_is${function}_L1($i)";
                if ($@) {
                    fail($@);
                }
                else {
                    my $truth = truth($matches && $i < 256);
                    is ($ret, $truth, "is${function}_L1( $display_name ) == $truth");
                }
            }

            if (defined $locale) {
                require locale; import locale;

                POSIX::setlocale( &POSIX::LC_ALL, "C");
                $ret = truth eval "test_is${function}_LC($i)";
                if ($@) {
                    fail($@);
                }
                else {
                    my $truth = truth($matches && $i < 128);
                    is ($ret, $truth, "is${function}_LC( $display_name ) == $truth (C locale)");
                }
            }

            if (defined $utf8_locale) {
                use locale;

                POSIX::setlocale( &POSIX::LC_ALL, $utf8_locale);
                $ret = truth eval "test_is${function}_LC($i)";
                if ($@) {
                    fail($@);
                }
                else {

                    # UTF-8 locale works on full range 0-255
                    my $truth = truth($matches && $i < 256);
                    is ($ret, $truth, "is${function}_LC( $display_name ) == $truth ($utf8_locale)");
                }
            }
        }

        $ret = truth eval "test_is${function}_uni($i)";
        if ($@) {
            fail($@);
        }
        else {
            is ($ret, $matches, "is${function}_uni( $display_name ) == $matches");
        }

        if (defined $locale && $name ne 'vertws') {
            require locale; import locale;

            POSIX::setlocale( &POSIX::LC_ALL, "C");
            $ret = truth eval "test_is${function}_LC_uvchr('$i')";
            if ($@) {
                fail($@);
            }
            else {
                my $truth = truth($matches && ($i < 128 || $i > 255));
                is ($ret, $truth, "is${function}_LC_uvchr( $display_name ) == $truth (C locale)");
            }
        }

        if (defined $utf8_locale && $name ne 'vertws') {
            use locale;

            POSIX::setlocale( &POSIX::LC_ALL, $utf8_locale);
            $ret = truth eval "test_is${function}_LC_uvchr('$i')";
            if ($@) {
                fail($@);
            }
            else {
                my $truth = truth($matches);
                is ($ret, $truth, "is${function}_LC_uvchr( $display_name ) == $truth ($utf8_locale)");
            }
        }

        my $char = chr($i);
        utf8::upgrade($char);
        $char = quotemeta $char if $char eq '\\' || $char eq "'";
        $ret = truth eval "test_is${function}_utf8('$char')";
        if ($@) {
            fail($@);
        }
        else {
            is ($ret, $matches, "is${function}_utf8( $display_name ) == $matches");
        }

        if ($name ne 'vertws' && defined $locale) {
            require locale; import locale;

            POSIX::setlocale( &POSIX::LC_ALL, "C");
            $ret = truth eval "test_is${function}_LC_utf8('$char')";
            if ($@) {
                fail($@);
            }
            else {
                my $truth = truth($matches && ($i < 128 || $i > 255));
                is ($ret, $truth, "is${function}_LC_utf8( $display_name ) == $truth (C locale)");
            }
        }

        if ($name ne 'vertws' && defined $utf8_locale) {
            use locale;

            POSIX::setlocale( &POSIX::LC_ALL, $utf8_locale);
            $ret = truth eval "test_is${function}_LC_utf8('$char')";
            if ($@) {
                fail($@);
            }
            else {
                my $truth = truth($matches);
                is ($ret, $truth, "is${function}_LC_utf8( $display_name ) == $truth ($utf8_locale)");
            }
        }
    }
}

my %to_properties = (
                FOLD => 'Case_Folding',
                LOWER => 'Lowercase_Mapping',
                TITLE => 'Titlecase_Mapping',
                UPPER => 'Uppercase_Mapping',
            );


foreach my $name (sort keys %to_properties) {
    my $property = $to_properties{$name};
    my ($list_ref, $map_ref, $format, $missing)
                                      = prop_invmap($property, );
    if (! $list_ref || ! $map_ref) {
        fail("No inversion map found for $property");
        next;
    }
    if ($format ne "al") {
        fail("Unexpected inversion map format ('$format') found for $property");
        next;
    }

    # Include all the Latin1 code points, plus 0x100.
    my @code_points = (0 .. 256);

    # Then include the next few multi-char folds above those from this
    # property, and include the next few single folds as well
    my $above_latins = 0;
    my $multi_char = 0;
    for my $i (0 .. @{$list_ref} - 1) {
        my $range_start = $list_ref->[$i];
        next if $range_start < 257;
        if (ref $map_ref->[$i] && $multi_char < 5)  {
            push @code_points, $range_start - 1 if $code_points[-1] != $range_start - 1;
            push @code_points, $range_start;
            $multi_char++;
        }
        elsif ($above_latins < 5) {
            push @code_points, $range_start - 1 if $code_points[-1] != $range_start - 1;
            push @code_points, $range_start;
            $above_latins++;
        }
        last if $above_latins >= 5 && $multi_char >= 5;
    }

    # And finally one non-Unicode code point.
    push @code_points, 0x110000;    # Above Unicode, no prop should match
    no warnings 'non_unicode';

    # $j is native; $i unicode.
    for my $j (@code_points) {
        my $i = utf8::native_to_unicode($j);
        my $function = $name;

        my $index = Unicode::UCD::search_invlist(\@{$list_ref}, $j);

        my $ret;
        my $char_name = charnames::viacode($j) // "No name";
        my $display_name = sprintf "\\N{U+%02X, %s}", $i, $char_name;

        # Test the base function
        $ret = eval "test_to${function}($j)";
        if ($@) {
            fail($@);
        }
        else {
            my $should_be = ($i < 128 && $map_ref->[$index] != $missing)
                             ? $map_ref->[$index] + $j - $list_ref->[$index]
                             : $j;
            is ($ret, $should_be, sprintf("to${function}( $display_name ) == 0x%02X", $should_be));
        }

        # Test _L1
        if ($name eq 'LOWER') {
            $ret = eval "test_to${function}_L1($j)";
            if ($@) {
                fail($@);
            }
            else {
                my $should_be = ($i < 256 && $map_ref->[$index] != $missing)
                                ? $map_ref->[$index] + $j - $list_ref->[$index]
                                : $j;
                is ($ret, $should_be, sprintf("to${function}_L1( $display_name ) == 0x%02X", $should_be));
            }
        }

        if ($name ne 'TITLE') { # Test _LC;  titlecase is not defined in locales.
            if (defined $locale) {
                require locale; import locale;

                    POSIX::setlocale( &POSIX::LC_ALL, "C");
                $ret = eval "test_to${function}_LC($j)";
                if ($@) {
                    fail($@);
                }
                else {
                    my $should_be = ($i < 128 && $map_ref->[$index] != $missing)
                                ? $map_ref->[$index] + $j - $list_ref->[$index]
                                : $j;
                    is ($ret, $should_be, sprintf("to${function}_LC( $display_name ) == 0x%02X (C locale)", $should_be));
                }
            }

            if (defined $utf8_locale) {
                use locale;

                SKIP: {
                    skip "to${property}_LC does not work for LATIN SMALL LETTER SHARP S", 1
                        if $j == 0xDF && ($name eq 'FOLD' || $name eq 'UPPER');

                    POSIX::setlocale( &POSIX::LC_ALL, $utf8_locale);
                    $ret = eval "test_to${function}_LC($j)";
                    if ($@) {
                        fail($@);
                    }
                    else {
                        my $should_be = ($i < 256
                                         && ! ref $map_ref->[$index]
                                         && $map_ref->[$index] != $missing
                                        )
                                        ? $map_ref->[$index] + $j - $list_ref->[$index]
                                        : $j;
                        is ($ret, $should_be, sprintf("to${function}_LC( $display_name ) == 0x%02X ($utf8_locale)", $should_be));
                    }
                }
            }
        }

        # The _uni and _utf8 functions return both the ordinal of the first
        # code point of the result, and the result in utf8.  The .xs tests
        # return these in an array, in [0] and [1] respectively, with [2] the
        # length of the utf8 in bytes.
        my $utf8_should_be = "";
        my $first_ord_should_be;
        if (ref $map_ref->[$index]) {   # A multi-char result
            for my $j (0 .. @{$map_ref->[$index]} - 1) {
                $utf8_should_be .= chr $map_ref->[$index][$j];
            }

            $first_ord_should_be = $map_ref->[$index][0];
        }
        else {  # A single-char result
            $first_ord_should_be = ($map_ref->[$index] != $missing)
                                    ? $map_ref->[$index] + $j - $list_ref->[$index]
                                    : $j;
            $utf8_should_be = chr $first_ord_should_be;
        }
        utf8::upgrade($utf8_should_be);

        # Test _uni
        my $s;
        my $len;
        $ret = eval "test_to${function}_uni($j)";
        if ($@) {
            fail($@);
        }
        else {
            is ($ret->[0], $first_ord_should_be, sprintf("to${function}_uni( $display_name ) == 0x%02X", $first_ord_should_be));
            is ($ret->[1], $utf8_should_be, sprintf("utf8 of to${function}_uni( $display_name )"));
            use bytes;
            is ($ret->[2], length $utf8_should_be, sprintf("number of bytes in utf8 of to${function}_uni( $display_name )"));
        }

        # Test _utf8
        my $char = chr($j);
        utf8::upgrade($char);
        $char = quotemeta $char if $char eq '\\' || $char eq "'";
        $ret = eval "test_to${function}_utf8('$char')";
        if ($@) {
            fail($@);
        }
        else {
            is ($ret->[0], $first_ord_should_be, sprintf("to${function}_utf8( $display_name ) == 0x%02X", $first_ord_should_be));
            is ($ret->[1], $utf8_should_be, sprintf("utf8 of to${function}_utf8( $display_name )"));
            use bytes;
            is ($ret->[2], length $utf8_should_be, sprintf("number of bytes in utf8 of to${function}_uni( $display_name )"));
        }

    }
}

# This is primarily to make sure that no non-Unicode warnings get generated
is(scalar @warnings, 0, "No warnings were generated " . join ", ", @warnings);

done_testing;
