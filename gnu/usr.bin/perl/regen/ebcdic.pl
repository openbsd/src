use v5.16.0;
use strict;
use warnings;

BEGIN { unshift @INC, '.' }

require './regen/regen_lib.pl';
require './regen/charset_translations.pl';

# Generates the EBCDIC translation tables that were formerly hard-coded into
# utfebcdic.h

my $out_fh = open_new('ebcdic_tables.h', '>',
        {style => '*', by => $0, });

sub output_table ($$;$) {
    my $table_ref = shift;
    my $name = shift;

    # Tables in hex easier to debug, but don't fit into 80 columns
    my $print_in_hex = shift // 1;

    die "Requres 256 entries in table $name, got @$table_ref" if @$table_ref != 256;

    print $out_fh "EXTCONST U8 $name\[\] = {\n";

    my $column_numbers= "/*_0   _1   _2   _3   _4   _5   _6   _7   _8   _9   _A   _B   _C   _D   _E  _F*/\n";
    print $out_fh $column_numbers if $print_in_hex;
    for my $i (0 .. 255) {
        if ($print_in_hex) {
            # No row headings, so will fit in 80 cols.
            #printf $out_fh "/* %X_ */ ", $i / 16 if $i % 16 == 0;
            printf $out_fh "0x%02X", $table_ref->[$i];
        }
        else {
            printf $out_fh "%4d", $table_ref->[$i];
        }
        print $out_fh ",", if $i < 255;
        #print $out_fh ($i < 255) ? "," : " ";
        #printf $out_fh " /* %X_ */", $i / 16 if $print_in_hex && $i % 16 == 15;
        print $out_fh "\n" if $i % 16 == 15;
    }
    print $out_fh $column_numbers if $print_in_hex;
    print $out_fh "};\n\n";
}

print $out_fh <<END;

#ifndef PERL_EBCDIC_TABLES_H_   /* Guard against nested #includes */
#define PERL_EBCDIC_TABLES_H_   1

/* This file contains definitions for various tables used in EBCDIC handling.
 * More info is in utfebcdic.h */
END

my @charsets = get_supported_code_pages();
shift @charsets;    # ASCII is the 0th, and we don't deal with that here.
foreach my $charset (@charsets) {
    # we process the whole array several times, make a copy
    my @a2e = @{get_a2n($charset)};

    print $out_fh "\n" . get_conditional_compile_line_start($charset);
    print $out_fh "\n";

    print $out_fh "/* Index is ASCII platform code point; value is $charset equivalent */\n";
    output_table(\@a2e, "PL_a2e");

    { # Construct the inverse
        my @e2a;
        for my $i (0 .. 255) {
            $e2a[$a2e[$i]] = $i;
        }
        print $out_fh "/* Index is $charset code point; value is ASCII platform equivalent */\n";
        output_table(\@e2a, "PL_e2a");
    }

    my @i82utf = @{get_I8_2_utf($charset)};
    print $out_fh <<END;
/* (Confusingly named) Index is $charset I8 byte; value is
 * $charset UTF-EBCDIC equivalent */
END
    output_table(\@i82utf, "PL_utf2e");

    { #Construct the inverse
        my @utf2i8;
        for my $i (0 .. 255) {
            $utf2i8[$i82utf[$i]] = $i;
        }
        print $out_fh <<END;
/* (Confusingly named) Index is $charset UTF-EBCDIC byte; value is
 * $charset I8 equivalent */
END
        output_table(\@utf2i8, "PL_e2utf");
    }

    {
        my @utf8skip;

        # These are invariants or continuation bytes.
        for my $i (0 .. 0xBF) {
            $utf8skip[$i82utf[$i]] = 1;
        }

        # These are start bytes;  The skip is the number of consecutive highest
        # order 1-bits (up to 7)
        for my $i (0xC0 .. 255) {
            my $count;
            if ($i == 0b11111111) {
                no warnings 'once';
                $count = $CHARSET_TRANSLATIONS::UTF_EBCDIC_MAXBYTES;
            }
            elsif (($i & 0b11111110) == 0b11111110) {
                $count= 7;
            }
            elsif (($i & 0b11111100) == 0b11111100) {
                $count= 6;
            }
            elsif (($i & 0b11111000) == 0b11111000) {
                $count= 5;
            }
            elsif (($i & 0b11110000) == 0b11110000) {
                $count= 4;
            }
            elsif (($i & 0b11100000) == 0b11100000) {
                $count= 3;
            }
            elsif (($i & 0b11000000) == 0b11000000) {
                $count= 2;
            }
            else {
                die "Something wrong for UTF8SKIP calculation for $i";
            }
            $utf8skip[$i82utf[$i]] = $count;
        }

        print $out_fh <<END;
/* Index is $charset UTF-EBCDIC byte; value is UTF8SKIP for start bytes;
 * 1 for continuation.  Adapted from the shadow flags table in tr16.  The
 * entries marked 9 in tr16 are continuation bytes and are marked as length 1
 * here so that we can recover. */
END
        output_table(\@utf8skip, "PL_utf8skip", 0);  # The 0 means don't print
                                                     # in hex
    }

    use feature 'unicode_strings';

    {
        my @lc;
        for my $i (0 .. 255) {
            $lc[$a2e[$i]] = $a2e[ord lc chr $i];
        }
        print $out_fh "/* Index is $charset code point; value is its lowercase equivalent */\n";
        output_table(\@lc, "PL_latin1_lc");
    }

    {
        my @uc;
        for my $i (0 .. 255) {
            my $uc = uc chr $i;
            if (length $uc > 1 || ord $uc > 255) {
                $uc = "\N{LATIN SMALL LETTER Y WITH DIAERESIS}";
            }
            $uc[$a2e[$i]] = $a2e[ord $uc];
        }
        print $out_fh <<END;
/* Index is $charset code point; value is its uppercase equivalent.
 * The 'mod' in the name means that codepoints whose uppercase is above 255 or
 * longer than 1 character map to LATIN SMALL LETTER Y WITH DIARESIS */
END
        output_table(\@uc, "PL_mod_latin1_uc");
    }

    { # PL_fold
        my @ascii_fold;
        for my $i (0 .. 255) {  # Initialise to identity map
            $ascii_fold[$i] = $i;
        }

        # Overwrite the entries that aren't identity
        for my $chr ('A' .. 'Z') {
            $ascii_fold[$a2e[ord $chr]] = $a2e[ord lc $chr];
        }
        for my $chr ('a' .. 'z') {
            $ascii_fold[$a2e[ord $chr]] = $a2e[ord uc $chr];
        }
        print $out_fh <<END;
/* Index is $charset code point; For A-Z, value is a-z; for a-z, value
 * is A-Z; all other code points map to themselves */
END
        output_table(\@ascii_fold, "PL_fold");
    }

    {
        my @latin1_fold;
        for my $i (0 .. 255) {
            my $char = chr $i;
            my $lc = lc $char;

            # lc and uc adequately proxy for fold-case pairs in this 0-255
            # range
            my $uc = uc $char;
            $uc = $char if length $uc > 1 || ord $uc > 255;
            if ($lc ne $char) {
                $latin1_fold[$a2e[$i]] = $a2e[ord $lc];
            }
            elsif ($uc ne $char) {
                $latin1_fold[$a2e[$i]] = $a2e[ord $uc];
            }
            else {
                $latin1_fold[$a2e[$i]] = $a2e[$i];
            }
        }
        print $out_fh <<END;
/* Index is $charset code point; value is its other fold-pair equivalent
 * (A => a; a => A, etc) in the 0-255 range.  If no such equivalent, value is
 * the code point itself */
END
        output_table(\@latin1_fold, "PL_fold_latin1");
    }

    print $out_fh get_conditional_compile_line_end();
}

print $out_fh "\n#endif /* PERL_EBCDIC_TABLES_H_ */\n";

read_only_bottom_close_and_rename($out_fh);
