use v5.16.0;
use strict;
use warnings;
require './regen/regen_lib.pl';
require './regen/charset_translations.pl';
use Unicode::UCD;
use charnames qw(:loose);

my $out_fh = open_new('unicode_constants.h', '>',
        {style => '*', by => $0,
                      from => "Unicode data"});

print $out_fh <<END;

#ifndef PERL_UNICODE_CONSTANTS_H_   /* Guard against nested #includes */
#define PERL_UNICODE_CONSTANTS_H_   1

/* This file contains #defines for the version of Unicode being used and
 * various Unicode code points.  The values the code point macros expand to
 * are the native Unicode code point, or all or portions of the UTF-8 encoding
 * for the code point.  In the former case, the macro name has the suffix
 * "_NATIVE"; otherwise, the suffix "_UTF8".
 *
 * The macros that have the suffix "_UTF8" may have further suffixes, as
 * follows:
 *  "_FIRST_BYTE" if the value is just the first byte of the UTF-8
 *                representation; the value will be a numeric constant.
 *  "_TAIL"       if instead it represents all but the first byte.  This, and
 *                with no additional suffix are both string constants */

/*
=head1 Unicode Support

=for apidoc AmU|placeholder|BOM_UTF8

This is a macro that evaluates to a string constant of the  UTF-8 bytes that
define the Unicode BYTE ORDER MARK (U+FEFF) for the platform that perl
is compiled on.  This allows code to use a mnemonic for this character that
works on both ASCII and EBCDIC platforms.
S<C<sizeof(BOM_UTF8) - 1>> can be used to get its length in
bytes.

=for apidoc AmU|placeholder|REPLACEMENT_CHARACTER_UTF8

This is a macro that evaluates to a string constant of the  UTF-8 bytes that
define the Unicode REPLACEMENT CHARACTER (U+FFFD) for the platform that perl
is compiled on.  This allows code to use a mnemonic for this character that
works on both ASCII and EBCDIC platforms.
S<C<sizeof(REPLACEMENT_CHARACTER_UTF8) - 1>> can be used to get its length in
bytes.

=cut
*/

END

my $version = Unicode::UCD::UnicodeVersion();
my ($major, $dot, $dotdot) = $version =~ / (.*?) \. (.*?) (?: \. (.*) )? $ /x;
$dotdot = 0 unless defined $dotdot;

print $out_fh <<END;
#define UNICODE_MAJOR_VERSION   $major
#define UNICODE_DOT_VERSION     $dot
#define UNICODE_DOT_DOT_VERSION $dotdot

END

# The data are at __DATA__  in this file.

my @data = <DATA>;

foreach my $charset (get_supported_code_pages()) {
    print $out_fh "\n" . get_conditional_compile_line_start($charset);

    my @a2n = @{get_a2n($charset)};

    for ( @data ) {
        chomp;

        # Convert any '#' comments to /* ... */; empty lines and comments are
        # output as blank lines
        if ($_ =~ m/ ^ \s* (?: \# ( .* ) )? $ /x) {
            my $comment_body = $1 // "";
            if ($comment_body ne "") {
                print $out_fh "/* $comment_body */\n";
            }
            else {
                print $out_fh "\n";
            }
            next;
        }

        unless ($_ =~ m/ ^ ( [^\ ]* )           # Name or code point token
                        (?: [\ ]+ ( [^ ]* ) )?  # optional flag
                        (?: [\ ]+ ( .* ) )?  # name if unnamed; flag is required
                    /x)
        {
            die "Unexpected syntax at line $.: $_\n";
        }

        my $name_or_cp = $1;
        my $flag = $2;
        my $desired_name = $3;

        my $name;
        my $cp;
        my $U_cp;   # code point in Unicode (not-native) terms

        if ($name_or_cp =~ /^U\+(.*)/) {
            $U_cp = hex $1;
            $name = charnames::viacode($name_or_cp);
            if (! defined $name) {
                next if $flag =~ /skip_if_undef/;
                die "Unknown code point '$name_or_cp' at line $.: $_\n" unless $desired_name;
                $name = "";
            }
        }
        else {
            $name = $name_or_cp;
            die "Unknown name '$name' at line $.: $_\n" unless defined $name;
            $U_cp = charnames::vianame($name =~ s/_/ /gr);
        }

        $cp = ($U_cp < 256)
            ? $a2n[$U_cp]
            : $U_cp;

        $name = $desired_name if $name eq "" && $desired_name;
        $name =~ s/[- ]/_/g;   # The macro name can have no blanks nor dashes

        my $str;
        my $suffix;
        if (defined $flag && $flag eq 'native') {
            die "Are you sure you want to run this on an above-Latin1 code point?" if $cp > 0xff;
            $suffix = '_NATIVE';
            $str = sprintf "0x%02X", $cp;        # Is a numeric constant
        }
        else {
            $str = join "", map { sprintf "\\x%02X", ord $_ } split //, cp_2_utfbytes($U_cp, $charset);

            $suffix = '_UTF8';
            if (! defined $flag || $flag =~ /^ string (_skip_if_undef)? $/x) {
                $str = "\"$str\"";  # Will be a string constant
            } elsif ($flag eq 'tail') {
                    $str =~ s/\\x..//;  # Remove the first byte
                    $suffix .= '_TAIL';
                    $str = "\"$str\"";  # Will be a string constant
            }
            elsif ($flag eq 'first') {
                $str =~ s/ \\x ( .. ) .* /$1/x; # Get the two nibbles of the 1st byte
                $suffix .= '_FIRST_BYTE';
                $str = "0x$str";        # Is a numeric constant
            }
            else {
                die "Unknown flag at line $.: $_\n";
            }
        }
        printf $out_fh "#   define %s%s  %s    /* U+%04X */\n", $name, $suffix, $str, $U_cp;
    }

    my $max_PRINT_A = 0;
    for my $i (0x20 .. 0x7E) {
        $max_PRINT_A = $a2n[$i] if $a2n[$i] > $max_PRINT_A;
    }
    printf $out_fh "#   define MAX_PRINT_A_FOR_USE_ONLY_BY_REGCOMP_DOT_C   0x%02X   /* The max code point that isPRINT_A */\n", $max_PRINT_A;

    print $out_fh "\n" . get_conditional_compile_line_end();

}

use Unicode::UCD 'prop_invlist';

my $count = 0;
my @other_invlist = prop_invlist("Other");
for (my $i = 0; $i < @other_invlist; $i += 2) {
    $count += ((defined $other_invlist[$i+1])
              ? $other_invlist[$i+1]
              : 0x110000)
              - $other_invlist[$i];
}
printf $out_fh "\n/* The number of code points not matching \\pC */\n"
             . "#define NON_OTHER_COUNT_FOR_USE_ONLY_BY_REGCOMP_DOT_C  %d\n",
            0x110000 - $count;

# If this release has both the CWCM and CWCF properties, find the highest code
# point which changes under any case change.  We can use this to short-circuit
# code
my @cwcm = prop_invlist('CWCM');
if (@cwcm) {
    my @cwcf = prop_invlist('CWCF');
    if (@cwcf) {
        my $max = ($cwcm[-1] < $cwcf[-1])
                  ? $cwcf[-1]
                  : $cwcm[-1];
        printf $out_fh "\n/* The highest code point that has any type of case change */\n"
             . "#define HIGHEST_CASE_CHANGING_CP_FOR_USE_ONLY_BY_UTF8_DOT_C  0x%X\n",
            $max - 1;
    }
}

print $out_fh "\n#endif /* PERL_UNICODE_CONSTANTS_H_ */\n";

read_only_bottom_close_and_rename($out_fh);

# DATA FORMAT
#
# Note that any apidoc comments you want in the file need to be added to one
# of the prints above
#
# A blank line is output as-is.
# Comments (lines whose first non-blank is a '#') are converted to C-style,
# though empty comments are converted to blank lines.  Otherwise, each line
# represents one #define, and begins with either a Unicode character name with
# the blanks and dashes in it squeezed out or replaced by underscores; or it
# may be a hexadecimal Unicode code point of the form U+xxxx.  In the latter
# case, the name will be looked-up to use as the name of the macro.  In either
# case, the macro name will have suffixes as listed above, and all blanks and
# dashes will be replaced by underscores.
#
# Each line may optionally have one of the following flags on it, separated by
# white space from the initial token.
#   string  indicates that the output is to be of the string form
#           described in the comments above that are placed in the file.
#   string_skip_ifundef  is the same as 'string', but instead of dying if the
#           code point doesn't exist, the line is just skipped: no output is
#           generated for it
#   first   indicates that the output is to be of the FIRST_BYTE form.
#   tail    indicates that the output is of the _TAIL form.
#   native  indicates that the output is the code point, converted to the
#           platform's native character set if applicable
#
# If the code point has no official name, the desired name may be appended
# after the flag, which will be ignored if there is an official name.
#
# This program is used to make it convenient to create compile time constants
# of UTF-8, and to generate proper EBCDIC as well as ASCII without manually
# having to figure things out.

__DATA__
U+017F string

U+0300 string

U+0399 string
U+03BC string

U+1E9E string_skip_if_undef

U+FB05 string
U+FB06 string
U+0130 string
U+0131 string

U+2010 string
BOM first
BOM tail

BOM string

U+FFFD string

U+10FFFF string MAX_UNICODE

NBSP native
NBSP string

DEL native
CR  native
LF  native
VT  native
ESC native
U+00DF native
U+00E5 native
U+00C5 native
U+00FF native
U+00B5 native
