#!perl -w

use strict;
use Test::More;
use Config;

use XS::APItest;

use Unicode::UCD qw(prop_invlist);

sub truth($) {  # Converts values so is() works
    return (shift) ? 1 : 0;
}

my $locale;
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

    for my $i (@code_points) {
        my $function = uc($name);

        my $matches = Unicode::UCD::_search_invlist(\@invlist, $i);
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

                $ret = truth eval "test_is${function}_LC($i)";
                if ($@) {
                    fail($@);
                }
                else {
                    my $truth = truth($matches && $i < 128);
                    is ($ret, $truth, "is${function}_LC( $display_name ) == $truth");
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

            $ret = truth eval "test_is${function}_LC_uvchr('$i')";
            if ($@) {
                fail($@);
            }
            else {
                my $truth = truth($matches && ($i < 128 || $i > 255));
                is ($ret, $truth, "is${function}_LC_uvchr( $display_name ) == $truth");
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

            $ret = truth eval "test_is${function}_LC_utf8('$char')";
            if ($@) {
                fail($@);
            }
            else {
                my $truth = truth($matches && ($i < 128 || $i > 255));
                is ($ret, $truth, "is${function}_LC_utf8( $display_name ) == $truth");
            }
        }
    }
}

# This is primarily to make sure that no non-Unicode warnings get generated
is(scalar @warnings, 0, "No warnings were generated " . join ", ", @warnings);

done_testing;
