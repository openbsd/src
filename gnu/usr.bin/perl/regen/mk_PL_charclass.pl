#!perl -w
use v5.15.8;
use strict;
use warnings;
require 'regen/regen_lib.pl';
require 'regen/charset_translations.pl';

# This program outputs l1_charclass_tab.h, which defines the guts of the
# PL_charclass table.  Each line is a bit map of properties that the Unicode
# code point at the corresponding position in the table array has.  The first
# line corresponds to code point U+0000, NULL, the last line to U+00FF.  For
# an application to see if the code point "i" has a particular property, it
# just does
#    'PL_charclass[i] & BIT'
# The bit names are of the form '_CC_property_suffix', where 'CC' stands for
# character class, and 'property' is the corresponding property, and 'suffix'
# is one of '_A' to mean the property is true only if the corresponding code
# point is ASCII, and '_L1' means that the range includes any Latin1
# character (ISO-8859-1 including the C0 and C1 controls).  A property without
# these suffixes does not have different forms for both ranges.

# This program need be run only when adding new properties to it, or upon a
# new Unicode release, to make sure things haven't been changed by it.

my @properties = qw(
    NONLATIN1_SIMPLE_FOLD
    NONLATIN1_FOLD
    ALPHANUMERIC
    ALPHA
    ASCII
    BLANK
    CASED
    CHARNAME_CONT
    CNTRL
    DIGIT
    GRAPH
    IDFIRST
    LOWER
    NON_FINAL_FOLD
    PRINT
    PUNCT
    QUOTEMETA
    SPACE
    UPPER
    WORDCHAR
    XDIGIT
    VERTSPACE
    IS_IN_SOME_FOLD
    MNEMONIC_CNTRL
);

# Read in the case fold mappings.
my %folded_closure;
my %simple_folded_closure;
my @hex_non_final_folds;
my @non_latin1_simple_folds;
my @folds;
use Unicode::UCD;

BEGIN { # Have to do this at compile time because using user-defined \p{property}

    # Use the Unicode data file if we are on an ASCII platform (which its data
    # is for), and it is in the modern format (starting in Unicode 3.1.0) and
    # it is available.  This avoids being affected by potential bugs
    # introduced by other layers of Perl
    my $file="lib/unicore/CaseFolding.txt";

    if (ord('A') == 65
        && pack("C*", split /\./, Unicode::UCD::UnicodeVersion()) ge v3.1.0
        && open my $fh, "<", $file)
    {
        @folds = <$fh>;
    }
    else {
        my ($invlist_ref, $invmap_ref, undef, $default)
                                    = Unicode::UCD::prop_invmap('Case_Folding');
        for my $i (0 .. @$invlist_ref - 1 - 1) {
            next if $invmap_ref->[$i] == $default;
            my $adjust = -1;
            for my $j ($invlist_ref->[$i] .. $invlist_ref->[$i+1] -1) {
                $adjust++;

                # Single-code point maps go to a 'C' type
                if (! ref $invmap_ref->[$i]) {
                    push @folds, sprintf("%04X; C; %04X\n",
                                        $j,
                                        $invmap_ref->[$i] + $adjust);
                }
                else {  # Multi-code point maps go to 'F'.  prop_invmap()
                        # guarantees that no adjustment is needed for these,
                        # as the range will contain just one element
                    push @folds, sprintf("%04X; F; %s\n",
                                        $j,
                                        join " ", map { sprintf "%04X", $_ }
                                                        @{$invmap_ref->[$i]});
                }
            }
        }
    }

    for (@folds) {
        chomp;

        # Lines look like (without the initial '#'
        #0130; F; 0069 0307; # LATIN CAPITAL LETTER I WITH DOT ABOVE
        # Get rid of comments, ignore blank or comment-only lines
        my $line = $_ =~ s/ (?: \s* \# .* )? $ //rx;
        next unless length $line;
        my ($hex_from, $fold_type, @folded) = split /[\s;]+/, $line;

        my $from = hex $hex_from;

        # Perl only deals with S, C, and F folds
        next if $fold_type ne 'C' and $fold_type ne 'F' and $fold_type ne 'S';

        # Get each code point in the range that participates in this line's fold.
        # The hash has keys of each code point in the range, and values of what it
        # folds to and what folds to it
        for my $i (0 .. @folded - 1) {
            my $hex_fold = $folded[$i];
            my $fold = hex $hex_fold;
            if ($fold < 256) {
                push @{$folded_closure{$fold}}, $from;
                push @{$simple_folded_closure{$fold}}, $from if $fold_type ne 'F';
            }
            if ($from < 256) {
                push @{$folded_closure{$from}}, $fold;
                push @{$simple_folded_closure{$from}}, $fold if $fold_type ne 'F';
            }

            if (($fold_type eq 'C' || $fold_type eq 'S')
                && ($fold < 256 != $from < 256))
            {
                # Fold is simple (hence can't be a non-final fold, so the 'if'
                # above is mutualy exclusive from the 'if below) and crosses
                # 255/256 boundary.  We keep track of the Latin1 code points
                # in such folds.
                push @non_latin1_simple_folds, ($fold < 256)
                                                ? $fold
                                                : $from;
            }
            elsif ($i < @folded-1
                   && $fold < 256
                   && ! grep { $_ eq $hex_fold } @hex_non_final_folds)
            {
                push @hex_non_final_folds, $hex_fold;

                # Also add the upper case, which in the latin1 range folds to
                # $fold
                push @hex_non_final_folds, sprintf "%04X", ord uc chr $fold;
            }
        }
    }

    # Now having read all the lines, combine them into the full closure of each
    # code point in the range by adding lists together that share a common
    # element
    foreach my $folded (keys %folded_closure) {
        foreach my $from (grep { $_ < 256 } @{$folded_closure{$folded}}) {
            push @{$folded_closure{$from}}, @{$folded_closure{$folded}};
        }
    }
    foreach my $folded (keys %simple_folded_closure) {
        foreach my $from (grep { $_ < 256 } @{$simple_folded_closure{$folded}}) {
            push @{$simple_folded_closure{$from}}, @{$simple_folded_closure{$folded}};
        }
    }

    # We have the single-character folds that cross the 255/256, like KELVIN
    # SIGN => 'k', but we need the closure, so add like 'K' to it
    foreach my $folded (@non_latin1_simple_folds) {
        foreach my $fold (@{$simple_folded_closure{$folded}}) {
            if ($fold < 256 && ! grep { $fold == $_ } @non_latin1_simple_folds) {
                push @non_latin1_simple_folds, $fold;
            }
        }
    }
}

sub Is_Non_Latin1_Fold {
    my @return;

    foreach my $folded (keys %folded_closure) {
        push @return, sprintf("%X", $folded), if grep { $_ > 255 }
                                                     @{$folded_closure{$folded}};
    }
    return join("\n", @return) . "\n";
}

sub Is_Non_Latin1_Simple_Fold { # Latin1 code points that are folded to by
                                # non-Latin1 code points as single character
                                # folds
    return join("\n", map { sprintf "%X", $_ } @non_latin1_simple_folds) . "\n";
}

sub Is_Non_Final_Fold {
    return join("\n", @hex_non_final_folds) . "\n";
}

my @bits;   # Bit map for each code point

# For each character, calculate which properties it matches.
for my $ord (0..255) {
    my $char = chr($ord);
    utf8::upgrade($char);   # Important to use Unicode rules!

    # Look at all the properties we care about here.
    for my $property (@properties) {
        my $name = $property;

        # Remove the suffix to get the actual property name.
        # Currently the suffixes are '_L1', '_A', and none.
        # If is a latin1 version, no further checking is needed.
        if (! ($name =~ s/_L1$//)) {

            # Here, isn't an _L1.  If its _A, it's automatically false for
            # non-ascii.  The only current ones (besides ASCII) without a
            # suffix are valid over the whole range.
            next if $name =~ s/_A$// && $char !~ /\p{ASCII}/;
        }
        my $re;
        if ($name eq 'PUNCT') {;

            # Sadly, this is inconsistent: \pP and \pS for the ascii range,
            # just \pP outside it.
            $re = qr/\p{Punct}|[^\P{Symbol}\P{ASCII}]/;
        } elsif ($name eq 'CHARNAME_CONT') {;
            $re = qr/\p{_Perl_Charname_Continue}/,
        } elsif ($name eq 'SPACE') {;
            $re = qr/\p{XPerlSpace}/;
        } elsif ($name eq 'IDFIRST') {
            $re = qr/[_\p{XPosixAlpha}]/;
        } elsif ($name eq 'WORDCHAR') {
            $re = qr/\p{XPosixWord}/;
        } elsif ($name eq 'LOWER') {
            $re = qr/\p{XPosixLower}/;
        } elsif ($name eq 'UPPER') {
            $re = qr/\p{XPosixUpper}/;
        } elsif ($name eq 'ALPHANUMERIC') {
            # Like \w, but no underscore
            $re = qr/\p{Alnum}/;
        } elsif ($name eq 'ALPHA') {
            $re = qr/\p{XPosixAlpha}/;
        } elsif ($name eq 'QUOTEMETA') {
            $re = qr/\p{_Perl_Quotemeta}/;
        } elsif ($name eq 'NONLATIN1_FOLD') {
            $re = qr/\p{Is_Non_Latin1_Fold}/;
        } elsif ($name eq 'NONLATIN1_SIMPLE_FOLD') {
            $re = qr/\p{Is_Non_Latin1_Simple_Fold}/;
        } elsif ($name eq 'NON_FINAL_FOLD') {
            $re = qr/\p{Is_Non_Final_Fold}/;
        } elsif ($name eq 'IS_IN_SOME_FOLD') {
            $re = qr/\p{_Perl_Any_Folds}/;
        } elsif ($name eq 'MNEMONIC_CNTRL') {
            # These are the control characters that there are mnemonics for
            $re = qr/[\a\b\e\f\n\r\t]/;
        } else {    # The remainder have the same name and values as Unicode
            $re = eval "qr/\\p{$name}/";
            use Carp;
            carp $@ if ! defined $re;
        }
        #print STDERR __LINE__, ": $ord, $name $property, $re\n";
        if ($char =~ $re) {  # Add this property if matches
            $bits[$ord] .= '|' if $bits[$ord];
            $bits[$ord] .= "(1U<<_CC_$property)";
        }
    }
    #print __LINE__, " $ord $char $bits[$ord]\n";
}

my $out_fh = open_new('l1_char_class_tab.h', '>',
		      {style => '*', by => $0,
                      from => "property definitions"});

print $out_fh <<END;
/* For code points whose position is not the same as Unicode,  both are shown
 * in the comment*/
END

# Output the table using fairly short names for each char.
my $is_for_ascii = 1;   # get_supported_code_pages() returns the ASCII
                        # character set first
foreach my $charset (get_supported_code_pages()) {
    my @a2n = @{get_a2n($charset)};
    my @out;
    my @utf_to_i8;

    if ($is_for_ascii) {
        $is_for_ascii = 0;
    }
    else {  # EBCDIC.  Calculate mapping from UTF-EBCDIC bytes to I8
        my $i8_to_utf_ref = get_I8_2_utf($charset);
        for my $i (0..255) {
            $utf_to_i8[$i8_to_utf_ref->[$i]] = $i;
        }
    }

    print $out_fh "\n" . get_conditional_compile_line_start($charset);
    for my $ord (0..255) {
        my $name;
        my $char = chr $ord;
        if ($char =~ /\p{PosixGraph}/) {
            my $quote = $char eq "'" ? '"' : "'";
            $name = $quote . chr($ord) . $quote;
        }
        elsif ($char =~ /\p{XPosixGraph}/) {
            use charnames();
            $name = charnames::viacode($ord);
            $name =~ s/LATIN CAPITAL LETTER //
                    or $name =~ s/LATIN SMALL LETTER (.*)/\L$1/
                    or $name =~ s/ SIGN\b//
                    or $name =~ s/EXCLAMATION MARK/'!'/
                    or $name =~ s/QUESTION MARK/'?'/
                    or $name =~ s/QUOTATION MARK/QUOTE/
                    or $name =~ s/ INDICATOR//;
            $name =~ s/\bWITH\b/\L$&/;
            $name =~ s/\bONE\b/1/;
            $name =~ s/\b(TWO|HALF)\b/2/;
            $name =~ s/\bTHREE\b/3/;
            $name =~ s/\b QUARTER S? \b/4/x;
            $name =~ s/VULGAR FRACTION (.) (.)/$1\/$2/;
            $name =~ s/\bTILDE\b/'~'/i
                    or $name =~ s/\bCIRCUMFLEX\b/'^'/i
                    or $name =~ s/\bSTROKE\b/'\/'/i
                    or $name =~ s/ ABOVE\b//i;
        }
        else {
            use Unicode::UCD qw(prop_invmap);
            my ($list_ref, $map_ref, $format)
                   = prop_invmap("_Perl_Name_Alias", '_perl_core_internal_ok');
            if ($format !~ /^s/) {
                use Carp;
                carp "Unexpected format '$format' for '_Perl_Name_Alias";
                last;
            }
            my $which = Unicode::UCD::search_invlist($list_ref, $ord);
            if (! defined $which) {
                use Carp;
                carp "No name found for code pont $ord";
            }
            else {
                my $map = $map_ref->[$which];
                if (! ref $map) {
                    $name = $map;
                }
                else {
                    # Just pick the first abbreviation if more than one
                    my @names = grep { $_ =~ /abbreviation/ } @$map;
                    $name = $names[0];
                }
                $name =~ s/:.*//;
            }
        }

        my $index = $a2n[$ord];
        my $i8;
        $i8 = $utf_to_i8[$index] if @utf_to_i8;

        $out[$index] = "/* ";
        $out[$index] .= sprintf "0x%02X ", $index if $ord != $index;
        $out[$index] .= sprintf "U+%02X ", $ord;
        $out[$index] .= sprintf "I8=%02X ", $i8 if defined $i8 && $i8 != $ord;
        $out[$index] .= "$name */ ";
        $out[$index] .= $bits[$ord];

        # For EBCDIC character sets, we also add some data for when the bytes
        # are in UTF-EBCDIC; these are based on the fundamental
        # characteristics of UTF-EBCDIC.
        if (@utf_to_i8) {
            if ($i8 >= 0xC5 && $i8 != 0xE0) {
                $out[$index] .= '|(1U<<_CC_UTF8_IS_START)';
                if ($i8 <= 0xC7) {
                    $out[$index] .= '|(1U<<_CC_UTF8_IS_DOWNGRADEABLE_START)';
                }
            }
            if (($i8 & 0xE0) == 0xA0) {
                $out[$index] .= '|(1U<<_CC_UTF8_IS_CONTINUATION)';
            }
            if ($i8 >= 0xF1) {
                $out[$index] .=
                          '|(1U<<_CC_UTF8_START_BYTE_IS_FOR_AT_LEAST_SURROGATE)';
            }
        }

        $out[$index] .= ",\n";
    }
    $out[-1] =~ s/,$//;     # No trailing comma in the final entry

    print $out_fh join "", @out;
    print $out_fh "\n" . get_conditional_compile_line_end();
}

read_only_bottom_close_and_rename($out_fh)
