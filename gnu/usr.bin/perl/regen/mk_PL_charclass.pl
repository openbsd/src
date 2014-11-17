#!perl -w
use v5.15.8;
use strict;
use warnings;
require 'regen/regen_lib.pl';

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
    PSXSPC
    PUNCT
    QUOTEMETA
    SPACE
    UPPER
    WORDCHAR
    XDIGIT
    VERTSPACE
    IS_IN_SOME_FOLD
    BACKSLASH_FOO_LBRACE_IS_META
);

# Read in the case fold mappings.
my %folded_closure;
my @hex_non_final_folds;
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

        # Perl only deals with C and F folds
        next if $fold_type ne 'C' and $fold_type ne 'F';

        # Get each code point in the range that participates in this line's fold.
        # The hash has keys of each code point in the range, and values of what it
        # folds to and what folds to it
        for my $i (0 .. @folded - 1) {
            my $hex_fold = $folded[$i];
            my $fold = hex $hex_fold;
            push @{$folded_closure{$fold}}, $from if $fold < 256;
            push @{$folded_closure{$from}}, $fold if $from < 256;

            if ($i < @folded-1
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
}

sub Is_Non_Latin1_Fold {
    my @return;

    foreach my $folded (keys %folded_closure) {
        push @return, sprintf("%X", $folded), if grep { $_ > 255 }
                                                     @{$folded_closure{$folded}};
    }
    return join("\n", @return) . "\n";
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
            next if $name =~ s/_A$// && $ord >= 128;

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
            $re = qr/[_\p{Alpha}]/;
        } elsif ($name eq 'PSXSPC') {
            $re = qr/[\v\p{Space}]/;
        } elsif ($name eq 'WORDCHAR') {
            $re = qr/\p{XPosixWord}/;
        } elsif ($name eq 'ALPHANUMERIC') {
            # Like \w, but no underscore
            $re = qr/\p{Alnum}/;
        } elsif ($name eq 'QUOTEMETA') {
            $re = qr/\p{_Perl_Quotemeta}/;
        } elsif ($name eq 'NONLATIN1_FOLD') {
            $re = qr/\p{Is_Non_Latin1_Fold}/;
        } elsif ($name eq 'NON_FINAL_FOLD') {
            $re = qr/\p{Is_Non_Final_Fold}/;
        } elsif ($name eq 'IS_IN_SOME_FOLD') {
            $re = qr/\p{_Perl_Any_Folds}/;
        } elsif ($name eq 'BACKSLASH_FOO_LBRACE_IS_META') {

            # This is true for FOO where FOO is the varying character in:
            # \a{, \b{, \c{, ...
            # and the sequence has non-literal meaning to Perl; so it is true
            # for 'x' because \x{ is special, but not 'a' because \a{ isn't.
            $re = qr/[gkNopPx]/;
        } else {    # The remainder have the same name and values as Unicode
            $re = eval "qr/\\p{$name}/";
            use Carp;
            carp $@ if ! defined $re;
        }
        #print "$ord, $name $property, $re\n";
        if ($char =~ $re) {  # Add this property if matches
            $bits[$ord] .= '|' if $bits[$ord];
            $bits[$ord] .= "(1U<<_CC_$property)";
        }
    }
    #print __LINE__, " $ord $char $bits[$ord]\n";
}

# Names of C0 controls
my @C0 = qw (
                NUL
                SOH
                STX
                ETX
                EOT
                ENQ
                ACK
                BEL
                BS
                HT
                LF
                VT
                FF
                CR
                SO
                SI
                DLE
                DC1
                DC2
                DC3
                DC4
                NAK
                SYN
                ETB
                CAN
                EOM
                SUB
                ESC
                FS
                GS
                RS
                US
            );

# Names of C1 controls, plus the adjacent DEL
my @C1 = qw(
                DEL
                PAD
                HOP
                BPH
                NBH
                IND
                NEL
                SSA
                ESA
                HTS
                HTJ
                VTS
                PLD
                PLU
                RI 
                SS2
                SS3
                DCS
                PU1
                PU2
                STS
                CCH
                MW 
                SPA
                EPA
                SOS
                SGC
                SCI
                CSI
                ST 
                OSC
                PM 
                APC
            );

my $out_fh = open_new('l1_char_class_tab.h', '>',
		      {style => '*', by => $0,
                      from => "property definitions"});

# Output the table using fairly short names for each char.
for my $ord (0..255) {
    my $name;
    if ($ord < 32) {    # A C0 control
        $name = $C0[$ord];
    } elsif ($ord > 32 && $ord < 127) { # Graphic
        $name = "'" . chr($ord) . "'";
    } elsif ($ord >= 127 && $ord <= 0x9f) {
        $name = $C1[$ord - 127];    # A C1 control + DEL
    } else {    # SPACE, or, if Latin1, shorten the name */
        use charnames();
        $name = charnames::viacode($ord);
        $name =~ s/LATIN CAPITAL LETTER //
        || $name =~ s/LATIN SMALL LETTER (.*)/\L$1/;
    }
    printf $out_fh "/* U+%02X %s */ %s,\n", $ord, $name, $bits[$ord];
}

read_only_bottom_close_and_rename($out_fh)
