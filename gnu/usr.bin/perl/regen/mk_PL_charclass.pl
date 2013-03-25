#!perl -w
use 5.012;
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
    ALNUMC_A
    ALNUMC_L1
    ALPHA_A
    ALPHA_L1
    BLANK_A
    BLANK_L1
    CHARNAME_CONT
    CNTRL_A
    CNTRL_L1
    DIGIT_A
    GRAPH_A
    GRAPH_L1
    IDFIRST_A
    IDFIRST_L1
    LOWER_A
    LOWER_L1
    OCTAL_A
    PRINT_A
    PRINT_L1
    PSXSPC_A
    PSXSPC_L1
    PUNCT_A
    PUNCT_L1
    SPACE_A
    SPACE_L1
    UPPER_A
    UPPER_L1
    WORDCHAR_A
    WORDCHAR_L1
    XDIGIT_A
    QUOTEMETA
);

# Read in the case fold mappings.
my %folded_closure;
my $file="lib/unicore/CaseFolding.txt";
open my $fh, "<", $file or die "Failed to read '$file': $!";
while (<$fh>) {
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
    foreach my $hex_fold (@folded) {
        my $fold = hex $hex_fold;
        push @{$folded_closure{$fold}}, $from if $fold < 256;
        push @{$folded_closure{$from}}, $fold if $from < 256;
    }
}

# Now having read all the lines, combine them into the full closure of each
# code point in the range by adding lists together that share a common element
foreach my $folded (keys %folded_closure) {
    foreach my $from (grep { $_ < 256 } @{$folded_closure{$folded}}) {
        push @{$folded_closure{$from}}, @{$folded_closure{$folded}};
    }
}

my @bits;   # Bit map for each code point

foreach my $folded (keys %folded_closure) {
    $bits[$folded] = "_CC_NONLATIN1_FOLD" if grep { $_ > 255 }
                                                @{$folded_closure{$folded}};
}

# For each character, calculate which properties it matches.
for my $ord (0..255) {
    my $char = chr($ord);
    utf8::upgrade($char);   # Important to use Unicode semantics!

    # Look at all the properties we care about here.
    for my $property (@properties) {
        my $name = $property;

        # Remove the suffix to get the actual property name.
        # Currently the suffixes are '_L1', '_A', and none.
        # If is a latin1 version, no further checking is needed.
        if (! ($name =~ s/_L1$//)) {

            # Here, isn't an _L1.  If its _A, it's automatically false for
            # non-ascii.  The only one current one without a suffix is valid
            # over the whole range.
            next if $name =~ s/_A$// && $ord >= 128;

        }
        my $re;
        if ($name eq 'PUNCT') {;

            # Sadly, this is inconsistent: \pP and \pS for the ascii range,
            # just \pP outside it.
            $re = qr/\p{Punct}|[^\P{Symbol}\P{ASCII}]/;
        } elsif ($name eq 'CHARNAME_CONT') {;
            $re = qr/[-\w ():\xa0]/;
        } elsif ($name eq 'SPACE') {;
            $re = qr/\s/;
        } elsif ($name eq 'IDFIRST') {
            $re = qr/[_\p{Alpha}]/;
        } elsif ($name eq 'PSXSPC') {
            $re = qr/[\v\p{Space}]/;
        } elsif ($name eq 'WORDCHAR') {
            $re = qr/\w/;
        } elsif ($name eq 'ALNUMC') {
            # Like \w, but no underscore
            $re = qr/\p{Alnum}/;
        } elsif ($name eq 'OCTAL') {
            $re = qr/[0-7]/;
        } elsif ($name eq 'QUOTEMETA') {
            $re = qr/\p{_Perl_Quotemeta}/;
        } else {    # The remainder have the same name and values as Unicode
            $re = eval "qr/\\p{$name}/";
            use Carp;
            carp $@ if ! defined $re;
        }
        #print "$ord, $name $property, $re\n";
        if ($char =~ $re) {  # Add this property if matches
            $bits[$ord] .= '|' if $bits[$ord];
            $bits[$ord] .= "_CC_$property";
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
                      from => "property definitions and $file"});

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
