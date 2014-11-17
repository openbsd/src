use v5.16.0;
use strict;
use warnings;
require 'regen/regen_lib.pl';
use charnames qw(:loose);

my $out_fh = open_new('unicode_constants.h', '>',
		      {style => '*', by => $0,
                      from => "Unicode data"});

print $out_fh <<END;

#ifndef H_UNICODE_CONSTANTS   /* Guard against nested #includes */
#define H_UNICODE_CONSTANTS   1

/* This file contains #defines for various Unicode code points.  The values
 * the macros expand to are the native Unicode code point, or all or portions
 * of the UTF-8 encoding for the code point.  In the former case, the macro
 * name has the suffix "_NATIVE"; otherwise, the suffix "_UTF8".
 *
 * The macros that have the suffix "_UTF8" may have further suffixes, as
 * follows:
 *  "_FIRST_BYTE" if the value is just the first byte of the UTF-8
 *                representation; the value will be a numeric constant.
 *  "_TAIL"       if instead it represents all but the first byte.  This, and
 *                with no additional suffix are both string constants */

END

# The data are at the end of this file.  A blank line is output as-is.
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

while ( <DATA> ) {
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
    my $undef_ok = $desired_name || $flag =~ /skip_if_undef/;

    if ($name_or_cp =~ /^U\+(.*)/) {
        $U_cp = hex $1;
        $name = charnames::viacode($name_or_cp);
        if (! defined $name) {
            die "Unknown code point '$name_or_cp' at line $.: $_\n" unless $undef_ok;
            $name = "";
        }
        $cp = utf8::unicode_to_native($U_cp);
    }
    else {
        $name = $name_or_cp;
        $cp = charnames::vianame($name =~ s/_/ /gr);
        $U_cp = utf8::native_to_unicode($cp);
        die "Unknown name '$name' at line $.: $_\n" unless defined $name;
    }

    $name = $desired_name if $name eq "" && $desired_name;
    $name =~ s/[- ]/_/g;   # The macro name can have no blanks nor dashes

    my $str = join "", map { sprintf "\\x%02X", $_ }
                       unpack("U0C*", pack("U", $cp));

    my $suffix = '_UTF8';
    if (! defined $flag  || $flag =~ /^ string (_skip_if_undef)? $/x) {
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
    elsif ($flag eq 'native') {
        die "Are you sure you want to run this on an above-Latin1 code point?" if $cp > 0xff;
        $suffix = '_NATIVE';
        $str = sprintf "0x%02X", $cp;        # Is a numeric constant
    }
    else {
        die "Unknown flag at line $.: $_\n";
    }
    printf $out_fh "#define %s%s  %s    /* U+%04X */\n", $name, $suffix, $str, $U_cp;
}

print $out_fh "\n#endif /* H_UNICODE_CONSTANTS */\n";

read_only_bottom_close_and_rename($out_fh);

__DATA__
U+017F string

U+0300 string

U+0399 string
U+03BC string

U+1E9E string

U+FB05 string
U+FB06 string

U+2010 string
U+D800 first FIRST_SURROGATE
BOM first
BOM tail

DEL native
CR  native
LF  native
U+00DF native
U+00E5 native
U+00C5 native
U+00FF native
U+00B5 native
