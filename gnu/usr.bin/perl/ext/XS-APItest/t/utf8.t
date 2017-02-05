#!perl -w

use strict;
use Test::More;
$|=1;

no warnings 'deprecated'; # Some of the below are above IV_MAX on 32 bit
                          # machines, and that is tested elsewhere

use XS::APItest;

my $pound_sign = chr utf8::unicode_to_native(163);

sub isASCII { ord "A" == 65 }

sub display_bytes {
    my $string = shift;
    return   '"'
           . join("", map { sprintf("\\x%02x", ord $_) } split "", $string)
           . '"';
}

# This  test file can't use byte_utf8a_to_utf8n() from t/charset_tools.pl
# because that uses the same functions we are testing here.  So UTF-EBCDIC
# strings are hard-coded as I8 strings in this file instead, and we use array
# lookup to translate into the appropriate code page.

my @i8_to_native = (    # Only code page 1047 so far.
# _0   _1   _2   _3   _4   _5   _6   _7   _8   _9   _A   _B   _C   _D   _E   _F
0x00,0x01,0x02,0x03,0x37,0x2D,0x2E,0x2F,0x16,0x05,0x15,0x0B,0x0C,0x0D,0x0E,0x0F,
0x10,0x11,0x12,0x13,0x3C,0x3D,0x32,0x26,0x18,0x19,0x3F,0x27,0x1C,0x1D,0x1E,0x1F,
0x40,0x5A,0x7F,0x7B,0x5B,0x6C,0x50,0x7D,0x4D,0x5D,0x5C,0x4E,0x6B,0x60,0x4B,0x61,
0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0x7A,0x5E,0x4C,0x7E,0x6E,0x6F,
0x7C,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,
0xD7,0xD8,0xD9,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xAD,0xE0,0xBD,0x5F,0x6D,
0x79,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x91,0x92,0x93,0x94,0x95,0x96,
0x97,0x98,0x99,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xC0,0x4F,0xD0,0xA1,0x07,
0x20,0x21,0x22,0x23,0x24,0x25,0x06,0x17,0x28,0x29,0x2A,0x2B,0x2C,0x09,0x0A,0x1B,
0x30,0x31,0x1A,0x33,0x34,0x35,0x36,0x08,0x38,0x39,0x3A,0x3B,0x04,0x14,0x3E,0xFF,
0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x51,0x52,0x53,0x54,0x55,0x56,
0x57,0x58,0x59,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x70,0x71,0x72,0x73,
0x74,0x75,0x76,0x77,0x78,0x80,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x9A,0x9B,0x9C,
0x9D,0x9E,0x9F,0xA0,0xAA,0xAB,0xAC,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,
0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBE,0xBF,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,0xDA,0xDB,
0xDC,0xDD,0xDE,0xDF,0xE1,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,0xFA,0xFB,0xFC,0xFD,0xFE,
);

*I8_to_native = (isASCII)
                    ? sub { return shift }
                    : sub { return join "", map { chr $i8_to_native[ord $_] }
                                            split "", shift };

my $is64bit = length sprintf("%x", ~0) > 8;


# Test utf8n_to_uvchr().  These provide essentially complete code coverage.
# Copied from utf8.h
my $UTF8_ALLOW_EMPTY            = 0x0001;
my $UTF8_ALLOW_CONTINUATION     = 0x0002;
my $UTF8_ALLOW_NON_CONTINUATION = 0x0004;
my $UTF8_ALLOW_SHORT            = 0x0008;
my $UTF8_ALLOW_LONG             = 0x0010;
my $UTF8_DISALLOW_SURROGATE     = 0x0020;
my $UTF8_WARN_SURROGATE         = 0x0040;
my $UTF8_DISALLOW_NONCHAR       = 0x0080;
my $UTF8_WARN_NONCHAR           = 0x0100;
my $UTF8_DISALLOW_SUPER         = 0x0200;
my $UTF8_WARN_SUPER             = 0x0400;
my $UTF8_DISALLOW_ABOVE_31_BIT  = 0x0800;
my $UTF8_WARN_ABOVE_31_BIT      = 0x1000;
my $UTF8_CHECK_ONLY             = 0x2000;

# Test uvchr_to_utf8().
my $UNICODE_WARN_SURROGATE        = 0x0001;
my $UNICODE_WARN_NONCHAR          = 0x0002;
my $UNICODE_WARN_SUPER            = 0x0004;
my $UNICODE_WARN_ABOVE_31_BIT     = 0x0008;
my $UNICODE_DISALLOW_SURROGATE    = 0x0010;
my $UNICODE_DISALLOW_NONCHAR      = 0x0020;
my $UNICODE_DISALLOW_SUPER        = 0x0040;
my $UNICODE_DISALLOW_ABOVE_31_BIT = 0x0080;

my $look_for_everything_utf8n_to
                        = $UTF8_DISALLOW_SURROGATE
			| $UTF8_WARN_SURROGATE
			| $UTF8_DISALLOW_NONCHAR
			| $UTF8_WARN_NONCHAR
			| $UTF8_DISALLOW_SUPER
			| $UTF8_WARN_SUPER
			| $UTF8_DISALLOW_ABOVE_31_BIT
			| $UTF8_WARN_ABOVE_31_BIT;
my $look_for_everything_uvchr_to
                        = $UNICODE_DISALLOW_SURROGATE
			| $UNICODE_WARN_SURROGATE
			| $UNICODE_DISALLOW_NONCHAR
			| $UNICODE_WARN_NONCHAR
			| $UNICODE_DISALLOW_SUPER
			| $UNICODE_WARN_SUPER
			| $UNICODE_DISALLOW_ABOVE_31_BIT
			| $UNICODE_WARN_ABOVE_31_BIT;

foreach ([0, '', '', 'empty'],
	 [0, 'N', 'N', '1 char'],
	 [1, 'NN', 'N', '1 char substring'],
	 [-2, 'Perl', 'Rules', 'different'],
	 [0, $pound_sign, $pound_sign, 'pound sign'],
	 [1, $pound_sign . 10, $pound_sign . 1, '10 pounds is more than 1 pound'],
	 [1, $pound_sign . $pound_sign, $pound_sign, '2 pound signs are more than 1'],
	 [-2, ' $!', " \x{1F42B}!", 'Camels are worth more than 1 dollar'],
	 [-1, '!', "!\x{1F42A}", 'Initial substrings match'],
	) {
    my ($expect, $left, $right, $desc) = @$_;
    my $copy = $right;
    utf8::encode($copy);
    is(bytes_cmp_utf8($left, $copy), $expect, $desc);
    next if $right =~ tr/\0-\377//c;
    utf8::encode($left);
    is(bytes_cmp_utf8($right, $left), -$expect, "$desc reversed");
}

# The keys to this hash are Unicode code points, their values are the native
# UTF-8 representations of them.  The code points are chosen because they are
# "interesting" on either or both ASCII and EBCDIC platforms.  First we add
# boundaries where the number of bytes required to represent them increase, or
# are adjacent to problematic code points, so we want to make sure they aren't
# considered problematic.
my %code_points = (
    0x0100     => (isASCII) ? "\xc4\x80" : I8_to_native("\xc8\xa0"),
    0x0400 - 1 => (isASCII) ? "\xcf\xbf" : I8_to_native("\xdf\xbf"),
    0x0400     => (isASCII) ? "\xd0\x80" : I8_to_native("\xe1\xa0\xa0"),
    0x0800 - 1 => (isASCII) ? "\xdf\xbf"     : I8_to_native("\xe1\xbf\xbf"),
    0x0800     => (isASCII) ? "\xe0\xa0\x80" : I8_to_native("\xe2\xa0\xa0"),
    0x4000 - 1 => (isASCII) ? "\xe3\xbf\xbf" : I8_to_native("\xef\xbf\xbf"),
    0x4000     => (isASCII) ? "\xe4\x80\x80" : I8_to_native("\xf0\xb0\xa0\xa0"),
    0x8000 - 1 => (isASCII) ? "\xe7\xbf\xbf" : I8_to_native("\xf0\xbf\xbf\xbf"),

    # First code point that the implementation of isUTF8_POSSIBLY_PROBLEMATIC,
    # as of this writing, considers potentially problematic on EBCDIC
    0x8000     => (isASCII) ? "\xe8\x80\x80" : I8_to_native("\xf1\xa0\xa0\xa0"),

    0xD000 - 1 => (isASCII) ? "\xec\xbf\xbf" : I8_to_native("\xf1\xb3\xbf\xbf"),

    # First code point that the implementation of isUTF8_POSSIBLY_PROBLEMATIC,
    # as of this writing, considers potentially problematic on ASCII
    0xD000     => (isASCII) ? "\xed\x80\x80" : I8_to_native("\xf1\xb4\xa0\xa0"),

    # Bracket the surrogates
    0xD7FF	=> (isASCII) ? "\xed\x9f\xbf" : I8_to_native("\xf1\xb5\xbf\xbf"),
    0xE000	=> (isASCII) ? "\xee\x80\x80" : I8_to_native("\xf1\xb8\xa0\xa0"),

    # Bracket the 32 contiguous non characters
    0xFDCF	=> (isASCII) ? "\xef\xb7\x8f" : I8_to_native("\xf1\xbf\xae\xaf"),
    0xFDF0      => (isASCII) ? "\xef\xb7\xb0" : I8_to_native("\xf1\xbf\xaf\xb0"),

    # Mostly bracket non-characters, but some are transitions to longer
    # strings
    0xFFFD	=> (isASCII) ? "\xef\xbf\xbd" : I8_to_native("\xf1\xbf\xbf\xbd"),
    0x10000 - 1 => (isASCII) ? "\xef\xbf\xbf" : I8_to_native("\xf1\xbf\xbf\xbf"),
    0x10000     => (isASCII) ? "\xf0\x90\x80\x80" : I8_to_native("\xf2\xa0\xa0\xa0"),
    0x1FFFD     => (isASCII) ? "\xf0\x9f\xbf\xbd" : I8_to_native("\xf3\xbf\xbf\xbd"),
    0x20000     => (isASCII) ? "\xf0\xa0\x80\x80" : I8_to_native("\xf4\xa0\xa0\xa0"),
    0x2FFFD     => (isASCII) ? "\xf0\xaf\xbf\xbd" : I8_to_native("\xf5\xbf\xbf\xbd"),
    0x30000     => (isASCII) ? "\xf0\xb0\x80\x80" : I8_to_native("\xf6\xa0\xa0\xa0"),
    0x3FFFD     => (isASCII) ? "\xf0\xbf\xbf\xbd" : I8_to_native("\xf7\xbf\xbf\xbd"),
    0x40000 - 1 => (isASCII) ? "\xf0\xbf\xbf\xbf" : I8_to_native("\xf7\xbf\xbf\xbf"),
    0x40000     => (isASCII) ? "\xf1\x80\x80\x80" : I8_to_native("\xf8\xa8\xa0\xa0\xa0"),
    0x4FFFD	=> (isASCII) ? "\xf1\x8f\xbf\xbd" : I8_to_native("\xf8\xa9\xbf\xbf\xbd"),
    0x50000     => (isASCII) ? "\xf1\x90\x80\x80" : I8_to_native("\xf8\xaa\xa0\xa0\xa0"),
    0x5FFFD	=> (isASCII) ? "\xf1\x9f\xbf\xbd" : I8_to_native("\xf8\xab\xbf\xbf\xbd"),
    0x60000     => (isASCII) ? "\xf1\xa0\x80\x80" : I8_to_native("\xf8\xac\xa0\xa0\xa0"),
    0x6FFFD	=> (isASCII) ? "\xf1\xaf\xbf\xbd" : I8_to_native("\xf8\xad\xbf\xbf\xbd"),
    0x70000     => (isASCII) ? "\xf1\xb0\x80\x80" : I8_to_native("\xf8\xae\xa0\xa0\xa0"),
    0x7FFFD	=> (isASCII) ? "\xf1\xbf\xbf\xbd" : I8_to_native("\xf8\xaf\xbf\xbf\xbd"),
    0x80000     => (isASCII) ? "\xf2\x80\x80\x80" : I8_to_native("\xf8\xb0\xa0\xa0\xa0"),
    0x8FFFD	=> (isASCII) ? "\xf2\x8f\xbf\xbd" : I8_to_native("\xf8\xb1\xbf\xbf\xbd"),
    0x90000     => (isASCII) ? "\xf2\x90\x80\x80" : I8_to_native("\xf8\xb2\xa0\xa0\xa0"),
    0x9FFFD	=> (isASCII) ? "\xf2\x9f\xbf\xbd" : I8_to_native("\xf8\xb3\xbf\xbf\xbd"),
    0xA0000     => (isASCII) ? "\xf2\xa0\x80\x80" : I8_to_native("\xf8\xb4\xa0\xa0\xa0"),
    0xAFFFD	=> (isASCII) ? "\xf2\xaf\xbf\xbd" : I8_to_native("\xf8\xb5\xbf\xbf\xbd"),
    0xB0000     => (isASCII) ? "\xf2\xb0\x80\x80" : I8_to_native("\xf8\xb6\xa0\xa0\xa0"),
    0xBFFFD	=> (isASCII) ? "\xf2\xbf\xbf\xbd" : I8_to_native("\xf8\xb7\xbf\xbf\xbd"),
    0xC0000     => (isASCII) ? "\xf3\x80\x80\x80" : I8_to_native("\xf8\xb8\xa0\xa0\xa0"),
    0xCFFFD	=> (isASCII) ? "\xf3\x8f\xbf\xbd" : I8_to_native("\xf8\xb9\xbf\xbf\xbd"),
    0xD0000     => (isASCII) ? "\xf3\x90\x80\x80" : I8_to_native("\xf8\xba\xa0\xa0\xa0"),
    0xDFFFD	=> (isASCII) ? "\xf3\x9f\xbf\xbd" : I8_to_native("\xf8\xbb\xbf\xbf\xbd"),
    0xE0000     => (isASCII) ? "\xf3\xa0\x80\x80" : I8_to_native("\xf8\xbc\xa0\xa0\xa0"),
    0xEFFFD	=> (isASCII) ? "\xf3\xaf\xbf\xbd" : I8_to_native("\xf8\xbd\xbf\xbf\xbd"),
    0xF0000     => (isASCII) ? "\xf3\xb0\x80\x80" : I8_to_native("\xf8\xbe\xa0\xa0\xa0"),
    0xFFFFD	=> (isASCII) ? "\xf3\xbf\xbf\xbd" : I8_to_native("\xf8\xbf\xbf\xbf\xbd"),
    0x100000    => (isASCII) ? "\xf4\x80\x80\x80" : I8_to_native("\xf9\xa0\xa0\xa0\xa0"),
    0x10FFFD	=> (isASCII) ? "\xf4\x8f\xbf\xbd" : I8_to_native("\xf9\xa1\xbf\xbf\xbd"),
    0x110000    => (isASCII) ? "\xf4\x90\x80\x80" : I8_to_native("\xf9\xa2\xa0\xa0\xa0"),

    # Things that would be noncharacters if they were in Unicode, and might be
    # mistaken, if the C code is bad, to be nonchars
    0x11FFFE    => (isASCII) ? "\xf4\x9f\xbf\xbe" : I8_to_native("\xf9\xa3\xbf\xbf\xbe"),
    0x11FFFF    => (isASCII) ? "\xf4\x9f\xbf\xbf" : I8_to_native("\xf9\xa3\xbf\xbf\xbf"),
    0x20FFFE    => (isASCII) ? "\xf8\x88\x8f\xbf\xbe" : I8_to_native("\xfa\xa1\xbf\xbf\xbe"),
    0x20FFFF    => (isASCII) ? "\xf8\x88\x8f\xbf\xbf" : I8_to_native("\xfa\xa1\xbf\xbf\xbf"),

    0x200000 - 1 => (isASCII) ? "\xf7\xbf\xbf\xbf" : I8_to_native("\xf9\xbf\xbf\xbf\xbf"),
    0x200000     => (isASCII) ? "\xf8\x88\x80\x80\x80" : I8_to_native("\xfa\xa0\xa0\xa0\xa0"),
    0x400000 - 1 => (isASCII) ? "\xf8\x8f\xbf\xbf\xbf" : I8_to_native("\xfb\xbf\xbf\xbf\xbf"),
    0x400000     => (isASCII) ? "\xf8\x90\x80\x80\x80" : I8_to_native("\xfc\xa4\xa0\xa0\xa0\xa0"),
    0x4000000 - 1 => (isASCII) ? "\xfb\xbf\xbf\xbf\xbf" : I8_to_native("\xfd\xbf\xbf\xbf\xbf\xbf"),
    0x4000000     => (isASCII) ? "\xfc\x84\x80\x80\x80\x80" : I8_to_native("\xfe\xa2\xa0\xa0\xa0\xa0\xa0"),
    0x4000000 - 1 => (isASCII) ? "\xfb\xbf\xbf\xbf\xbf" : I8_to_native("\xfd\xbf\xbf\xbf\xbf\xbf"),
    0x4000000     => (isASCII) ? "\xfc\x84\x80\x80\x80\x80" : I8_to_native("\xfe\xa2\xa0\xa0\xa0\xa0\xa0"),
    0x40000000 - 1 => (isASCII) ? "\xfc\xbf\xbf\xbf\xbf\xbf" : I8_to_native("\xfe\xbf\xbf\xbf\xbf\xbf\xbf"),
    0x40000000     => (isASCII) ? "\xfd\x80\x80\x80\x80\x80" : I8_to_native("\xff\xa0\xa0\xa0\xa0\xa0\xa0\xa1\xa0\xa0\xa0\xa0\xa0\xa0"),
    0x80000000 - 1 => (isASCII) ? "\xfd\xbf\xbf\xbf\xbf\xbf" : I8_to_native("\xff\xa0\xa0\xa0\xa0\xa0\xa0\xa1\xbf\xbf\xbf\xbf\xbf\xbf"),
    0x80000000     => (isASCII) ? "\xfe\x82\x80\x80\x80\x80\x80" : I8_to_native("\xff\xa0\xa0\xa0\xa0\xa0\xa0\xa2\xa0\xa0\xa0\xa0\xa0\xa0"),
    0xFFFFFFFF     => (isASCII) ? "\xfe\x83\xbf\xbf\xbf\xbf\xbf" : I8_to_native("\xff\xa0\xa0\xa0\xa0\xa0\xa0\xa3\xbf\xbf\xbf\xbf\xbf\xbf"),
);

if ($is64bit) {
    no warnings qw(overflow portable);
    $code_points{0x100000000}        = (isASCII) ? "\xfe\x84\x80\x80\x80\x80\x80" : I8_to_native("\xff\xa0\xa0\xa0\xa0\xa0\xa0\xa4\xa0\xa0\xa0\xa0\xa0\xa0");
    $code_points{0x1000000000 - 1}   = (isASCII) ? "\xfe\xbf\xbf\xbf\xbf\xbf\xbf" : I8_to_native("\xff\xa0\xa0\xa0\xa0\xa0\xa1\xbf\xbf\xbf\xbf\xbf\xbf\xbf");
    $code_points{0x1000000000}       = (isASCII) ? "\xff\x80\x80\x80\x80\x80\x81\x80\x80\x80\x80\x80\x80" : I8_to_native("\xff\xa0\xa0\xa0\xa0\xa0\xa2\xa0\xa0\xa0\xa0\xa0\xa0\xa0");
    $code_points{0xFFFFFFFFFFFFFFFF} = (isASCII) ? "\xff\x80\x8f\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf" : I8_to_native("\xff\xaf\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf");
}

# Now add in entries for each of code points 0-255, which require special
# handling on EBCDIC.  Remember the keys are Unicode values, and the values
# are the native UTF-8.  For invariants, the bytes are just the native chr.

my $cp = 0;
while ($cp < ((isASCII) ? 128 : 160)) {   # This is from the definition of
                                        # invariant
    $code_points{$cp} = chr utf8::unicode_to_native($cp);
    $cp++;
}

# Done with the invariants.  Now do the variants.  All in this range are 2
# byte.  Again, we can't use the internal functions to generate UTF-8, as
# those are what we are trying to test.  In the loop, we know what range the
# continuation bytes can be in, and what the lowest start byte can be.  So we
# cycle through them.

my $first_continuation = (isASCII) ? 0x80 : 0xA0;
my $final_continuation = 0xBF;
my $start = (isASCII) ? 0xC2 : 0xC5;

my $continuation = $first_continuation - 1;

while ($cp < 255) {
    if (++$continuation > $final_continuation) {

        # Wrap to the next start byte when we reach the final continuation
        # byte possible
        $continuation = $first_continuation;
        $start++;
    }
    $code_points{$cp} = I8_to_native(chr($start) . chr($continuation));

    $cp++;
}

my @warnings;

use warnings 'utf8';
local $SIG{__WARN__} = sub { push @warnings, @_ };

# This set of tests looks for basic sanity, and lastly tests the bottom level
# decode routine for the given code point.  If the earlier tests for that code
# point fail, that one probably will too.  Malformations are tested in later
# segments of code.
for my $u (sort { utf8::unicode_to_native($a) <=> utf8::unicode_to_native($b) }
          keys %code_points)
{
    my $hex_u = sprintf("0x%02X", $u);
    my $n = utf8::unicode_to_native($u);
    my $hex_n = sprintf("0x%02X", $n);
    my $bytes = $code_points{$u};

    my $offskip_should_be;
    {
        no warnings qw(overflow portable);
        $offskip_should_be = (isASCII)
            ? ( $u < 0x80           ? 1 :
                $u < 0x800          ? 2 :
                $u < 0x10000        ? 3 :
                $u < 0x200000       ? 4 :
                $u < 0x4000000      ? 5 :
                $u < 0x80000000     ? 6 : (($is64bit)
                                        ? ($u < 0x1000000000 ? 7 : 13)
                                        : 7)
              )
            : ($u < 0xA0        ? 1 :
               $u < 0x400       ? 2 :
               $u < 0x4000      ? 3 :
               $u < 0x40000     ? 4 :
               $u < 0x400000    ? 5 :
               $u < 0x4000000   ? 6 :
               $u < 0x40000000  ? 7 : 14 );
    }

    # If this test fails, subsequent ones are meaningless.
    next unless is(test_OFFUNISKIP($u), $offskip_should_be,
                   "Verify OFFUNISKIP($hex_u) is $offskip_should_be");
    my $invariant = $offskip_should_be == 1;
    my $display_invariant = $invariant || 0;
    is(test_OFFUNI_IS_INVARIANT($u), $invariant,
       "Verify OFFUNI_IS_INVARIANT($hex_u) is $display_invariant");

    my $uvchr_skip_should_be = $offskip_should_be;
    next unless is(test_UVCHR_SKIP($n), $uvchr_skip_should_be,
                   "Verify UVCHR_SKIP($hex_n) is $uvchr_skip_should_be");
    is(test_UVCHR_IS_INVARIANT($n), $offskip_should_be == 1,
       "Verify UVCHR_IS_INVARIANT($hex_n) is $display_invariant");

    my $n_chr = chr $n;
    utf8::upgrade $n_chr;

    is(test_UTF8_SKIP($n_chr), $uvchr_skip_should_be,
        "Verify UTF8_SKIP(chr $hex_n) is $uvchr_skip_should_be");

    use bytes;
    for (my $j = 0; $j < length $n_chr; $j++) {
        my $b = substr($n_chr, $j, 1);
        my $hex_b = sprintf("\"\\x%02x\"", ord $b);

        my $byte_invariant = $j == 0 && $uvchr_skip_should_be == 1;
        my $display_byte_invariant = $byte_invariant || 0;
        next unless is(test_UTF8_IS_INVARIANT($b), $byte_invariant,
                       "   Verify UTF8_IS_INVARIANT($hex_b) for byte $j "
                     . "is $display_byte_invariant");

        my $is_start = $j == 0 && $uvchr_skip_should_be > 1;
        my $display_is_start = $is_start || 0;
        next unless is(test_UTF8_IS_START($b), $is_start,
                    "      Verify UTF8_IS_START($hex_b) is $display_is_start");

        my $is_continuation = $j != 0 && $uvchr_skip_should_be > 1;
        my $display_is_continuation = $is_continuation || 0;
        next unless is(test_UTF8_IS_CONTINUATION($b), $is_continuation,
                       "      Verify UTF8_IS_CONTINUATION($hex_b) is "
                     . "$display_is_continuation");

        my $is_continued = $uvchr_skip_should_be > 1;
        my $display_is_continued = $is_continued || 0;
        next unless is(test_UTF8_IS_CONTINUED($b), $is_continued,
                       "      Verify UTF8_IS_CONTINUED($hex_b) is "
                     . "$display_is_continued");

        my $is_downgradeable_start =    $n < 256
                                     && $uvchr_skip_should_be > 1
                                     && $j == 0;
        my $display_is_downgradeable_start = $is_downgradeable_start || 0;
        next unless is(test_UTF8_IS_DOWNGRADEABLE_START($b),
                       $is_downgradeable_start,
                       "      Verify UTF8_IS_DOWNGRADEABLE_START($hex_b) is "
                     . "$display_is_downgradeable_start");

        my $is_above_latin1 =  $n > 255 && $j == 0;
        my $display_is_above_latin1 = $is_above_latin1 || 0;
        next unless is(test_UTF8_IS_ABOVE_LATIN1($b),
                       $is_above_latin1,
                       "      Verify UTF8_IS_ABOVE_LATIN1($hex_b) is "
                     . "$display_is_above_latin1");

        my $is_possibly_problematic =  $j == 0
                                    && $n >= ((isASCII)
                                              ? 0xD000
                                              : 0x8000);
        my $display_is_possibly_problematic = $is_possibly_problematic || 0;
        next unless is(test_isUTF8_POSSIBLY_PROBLEMATIC($b),
                       $is_possibly_problematic,
                       "      Verify isUTF8_POSSIBLY_PROBLEMATIC($hex_b) is "
                     . "$display_is_above_latin1");
    }

    # We are not trying to look for warnings, etc, so if they should occur, it
    # is an error.  But some of the code points here do cause warnings, so we
    # check here and turn off the ones that apply to such code points.  A
    # later section of the code tests for these kinds of things.
    my $this_utf8_flags = $look_for_everything_utf8n_to;
    my $len = length $bytes;
    if ($n > 2 ** 31 - 1) {
        $this_utf8_flags &=
                        ~($UTF8_DISALLOW_ABOVE_31_BIT|$UTF8_WARN_ABOVE_31_BIT);
    }
    if ($n > 0x10FFFF) {
        $this_utf8_flags &= ~($UTF8_DISALLOW_SUPER|$UTF8_WARN_SUPER);
    }
    elsif (($n & 0xFFFE) == 0xFFFE) {
        $this_utf8_flags &= ~($UTF8_DISALLOW_NONCHAR|$UTF8_WARN_NONCHAR);
    }

    undef @warnings;

    my $display_flags = sprintf "0x%x", $this_utf8_flags;
    my $ret_ref = test_utf8n_to_uvchr($bytes, $len, $this_utf8_flags);
    my $display_bytes = display_bytes($bytes);
    is($ret_ref->[0], $n, "Verify utf8n_to_uvchr($display_bytes, $display_flags) returns $hex_n");
    is($ret_ref->[1], $len, "Verify utf8n_to_uvchr() for $hex_n returns expected length");

    unless (is(scalar @warnings, 0,
               "Verify utf8n_to_uvchr() for $hex_n generated no warnings"))
    {
        diag "The warnings were: " . join(", ", @warnings);
    }

    undef @warnings;

    $ret_ref = test_valid_utf8_to_uvchr($bytes);
    is($ret_ref->[0], $n, "Verify valid_utf8_to_uvchr($display_bytes) returns $hex_n");
    is($ret_ref->[1], $len, "Verify valid_utf8_to_uvchr() for $hex_n returns expected length");

    unless (is(scalar @warnings, 0,
               "Verify valid_utf8_to_uvchr() for $hex_n generated no warnings"))
    {
        diag "The warnings were: " . join(", ", @warnings);
    }

    # Similarly for uvchr_to_utf8
    my $this_uvchr_flags = $look_for_everything_uvchr_to;
    if ($n > 2 ** 31 - 1) {
        $this_uvchr_flags &=
                ~($UNICODE_DISALLOW_ABOVE_31_BIT|$UNICODE_WARN_ABOVE_31_BIT);
    }
    if ($n > 0x10FFFF) {
        $this_uvchr_flags &= ~($UNICODE_DISALLOW_SUPER|$UNICODE_WARN_SUPER);
    }
    elsif (($n & 0xFFFE) == 0xFFFE) {
        $this_uvchr_flags &= ~($UNICODE_DISALLOW_NONCHAR|$UNICODE_WARN_NONCHAR);
    }
    $display_flags = sprintf "0x%x", $this_uvchr_flags;

    undef @warnings;

    my $ret = test_uvchr_to_utf8_flags($n, $this_uvchr_flags);
    ok(defined $ret, "Verify uvchr_to_utf8_flags($hex_n, $display_flags) returned success");
    is($ret, $bytes, "Verify uvchr_to_utf8_flags($hex_n, $display_flags) returns correct bytes");

    unless (is(scalar @warnings, 0,
        "Verify uvchr_to_utf8_flags($hex_n, $display_flags) for $hex_n generated no warnings"))
    {
        diag "The warnings were: " . join(", ", @warnings);
    }
}

my $REPLACEMENT = 0xFFFD;

# Now test the malformations.  All these raise category utf8 warnings.
my $c = (isASCII) ? "\x80" : "\xa0";    # A continuation byte
my @malformations = (
    [ "zero length string malformation", "", 0,
        $UTF8_ALLOW_EMPTY, 0, 0,
        qr/empty string/
    ],
    [ "orphan continuation byte malformation", I8_to_native("${c}a"),
        2,
        $UTF8_ALLOW_CONTINUATION, $REPLACEMENT, 1,
        qr/unexpected continuation byte/
    ],
    [ "premature next character malformation (immediate)",
        (isASCII) ? "\xc2a" : I8_to_native("\xc5") ."a",
        2,
        $UTF8_ALLOW_NON_CONTINUATION, $REPLACEMENT, 1,
        qr/unexpected non-continuation byte.*immediately after start byte/
    ],
    [ "premature next character malformation (non-immediate)",
        I8_to_native("\xf0${c}a"),
        3,
        $UTF8_ALLOW_NON_CONTINUATION, $REPLACEMENT, 2,
        qr/unexpected non-continuation byte .* 2 bytes after start byte/
    ],
    [ "too short malformation", I8_to_native("\xf0${c}a"), 2,
        # Having the 'a' after this, but saying there are only 2 bytes also
        # tests that we pay attention to the passed in length
        $UTF8_ALLOW_SHORT, $REPLACEMENT, 2,
        qr/2 bytes, need 4/
    ],
    [ "overlong malformation", I8_to_native("\xc0$c"), 2,
        $UTF8_ALLOW_LONG,
        0,   # NUL
        2,
        qr/2 bytes, need 1/
    ],
    [ "overflow malformation",
                    # These are the smallest overflowing on 64 byte machines:
                    # 2**64
        (isASCII) ? "\xff\x80\x90\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0"
                  : I8_to_native("\xff\xB0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0"),
        (isASCII) ? 13 : 14,
        0,  # There is no way to allow this malformation
        $REPLACEMENT,
        (isASCII) ? 13 : 14,
        qr/overflow/
    ],
);

foreach my $test (@malformations) {
    my ($testname, $bytes, $length, $allow_flags, $allowed_uv, $expected_len, $message ) = @$test;

    next if ! ok(length($bytes) >= $length, "$testname: Make sure won't read beyond buffer: " . length($bytes) . " >= $length");

    # Test what happens when this malformation is not allowed
    undef @warnings;
    my $ret_ref = test_utf8n_to_uvchr($bytes, $length, 0);
    is($ret_ref->[0], 0, "$testname: disallowed: Returns 0");
    is($ret_ref->[1], $expected_len, "$testname: disallowed: Returns expected length");
    if (is(scalar @warnings, 1, "$testname: disallowed: Got a single warning ")) {
        like($warnings[0], $message, "$testname: disallowed: Got expected warning");
    }
    else {
        if (scalar @warnings) {
            diag "The warnings were: " . join(", ", @warnings);
        }
    }

    {   # Next test when disallowed, and warnings are off.
        undef @warnings;
        no warnings 'utf8';
        my $ret_ref = test_utf8n_to_uvchr($bytes, $length, 0);
        is($ret_ref->[0], 0, "$testname: disallowed: no warnings 'utf8': Returns 0");
        is($ret_ref->[1], $expected_len, "$testname: disallowed: no warnings 'utf8': Returns expected length");
        if (!is(scalar @warnings, 0, "$testname: disallowed: no warnings 'utf8': no warnings generated")) {
            diag "The warnings were: " . join(", ", @warnings);
        }
    }

    # Test with CHECK_ONLY
    undef @warnings;
    $ret_ref = test_utf8n_to_uvchr($bytes, $length, $UTF8_CHECK_ONLY);
    is($ret_ref->[0], 0, "$testname: CHECK_ONLY: Returns 0");
    is($ret_ref->[1], -1, "$testname: CHECK_ONLY: returns expected length");
    if (! is(scalar @warnings, 0, "$testname: CHECK_ONLY: no warnings generated")) {
        diag "The warnings were: " . join(", ", @warnings);
    }

    next if $allow_flags == 0;    # Skip if can't allow this malformation

    # Test when the malformation is allowed
    undef @warnings;
    $ret_ref = test_utf8n_to_uvchr($bytes, $length, $allow_flags);
    is($ret_ref->[0], $allowed_uv, "$testname: allowed: Returns expected uv");
    is($ret_ref->[1], $expected_len, "$testname: allowed: Returns expected length");
    if (!is(scalar @warnings, 0, "$testname: allowed: no warnings generated"))
    {
        diag "The warnings were: " . join(", ", @warnings);
    }
}

# Now test the cases where a legal code point is generated, but may or may not
# be allowed/warned on.
my @tests = (
    [ "lowest surrogate",
        (isASCII) ? "\xed\xa0\x80" : I8_to_native("\xf1\xb6\xa0\xa0"),
        $UTF8_WARN_SURROGATE, $UTF8_DISALLOW_SURROGATE,
        'surrogate', 0xD800,
        (isASCII) ? 3 : 4,
        qr/surrogate/
    ],
    [ "a middle surrogate",
        (isASCII) ? "\xed\xa4\x8d" : I8_to_native("\xf1\xb6\xa8\xad"),
        $UTF8_WARN_SURROGATE, $UTF8_DISALLOW_SURROGATE,
        'surrogate', 0xD90D,
        (isASCII) ? 3 : 4,
        qr/surrogate/
    ],
    [ "highest surrogate",
        (isASCII) ? "\xed\xbf\xbf" : I8_to_native("\xf1\xb7\xbf\xbf"),
        $UTF8_WARN_SURROGATE, $UTF8_DISALLOW_SURROGATE,
        'surrogate', 0xDFFF,
        (isASCII) ? 3 : 4,
        qr/surrogate/
    ],
    [ "first non_unicode",
        (isASCII) ? "\xf4\x90\x80\x80" : I8_to_native("\xf9\xa2\xa0\xa0\xa0"),
        $UTF8_WARN_SUPER, $UTF8_DISALLOW_SUPER,
        'non_unicode', 0x110000,
        (isASCII) ? 4 : 5,
        qr/not Unicode.* may not be portable/
    ],
    [ "first of 32 consecutive non-character code points",
        (isASCII) ? "\xef\xb7\x90" : I8_to_native("\xf1\xbf\xae\xb0"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xFDD0,
        (isASCII) ? 3 : 4,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "a mid non-character code point of the 32 consecutive ones",
        (isASCII) ? "\xef\xb7\xa0" : I8_to_native("\xf1\xbf\xaf\xa0"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xFDE0,
        (isASCII) ? 3 : 4,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "final of 32 consecutive non-character code points",
        (isASCII) ? "\xef\xb7\xaf" : I8_to_native("\xf1\xbf\xaf\xaf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xFDEF,
        (isASCII) ? 3 : 4,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+FFFE",
        (isASCII) ? "\xef\xbf\xbe" : I8_to_native("\xf1\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xFFFE,
        (isASCII) ? 3 : 4,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+FFFF",
        (isASCII) ? "\xef\xbf\xbf" : I8_to_native("\xf1\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xFFFF,
        (isASCII) ? 3 : 4,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+1FFFE",
        (isASCII) ? "\xf0\x9f\xbf\xbe" : I8_to_native("\xf3\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x1FFFE, 4,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+1FFFF",
        (isASCII) ? "\xf0\x9f\xbf\xbf" : I8_to_native("\xf3\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x1FFFF, 4,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+2FFFE",
        (isASCII) ? "\xf0\xaf\xbf\xbe" : I8_to_native("\xf5\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x2FFFE, 4,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+2FFFF",
        (isASCII) ? "\xf0\xaf\xbf\xbf" : I8_to_native("\xf5\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x2FFFF, 4,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+3FFFE",
        (isASCII) ? "\xf0\xbf\xbf\xbe" : I8_to_native("\xf7\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x3FFFE, 4,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+3FFFF",
        (isASCII) ? "\xf0\xbf\xbf\xbf" : I8_to_native("\xf7\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x3FFFF, 4,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+4FFFE",
        (isASCII) ? "\xf1\x8f\xbf\xbe" : I8_to_native("\xf8\xa9\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x4FFFE,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+4FFFF",
        (isASCII) ? "\xf1\x8f\xbf\xbf" : I8_to_native("\xf8\xa9\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x4FFFF,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+5FFFE",
        (isASCII) ? "\xf1\x9f\xbf\xbe" : I8_to_native("\xf8\xab\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x5FFFE,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+5FFFF",
        (isASCII) ? "\xf1\x9f\xbf\xbf" : I8_to_native("\xf8\xab\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x5FFFF,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+6FFFE",
        (isASCII) ? "\xf1\xaf\xbf\xbe" : I8_to_native("\xf8\xad\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x6FFFE,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+6FFFF",
        (isASCII) ? "\xf1\xaf\xbf\xbf" : I8_to_native("\xf8\xad\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x6FFFF,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+7FFFE",
        (isASCII) ? "\xf1\xbf\xbf\xbe" : I8_to_native("\xf8\xaf\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x7FFFE,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+7FFFF",
        (isASCII) ? "\xf1\xbf\xbf\xbf" : I8_to_native("\xf8\xaf\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x7FFFF,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+8FFFE",
        (isASCII) ? "\xf2\x8f\xbf\xbe" : I8_to_native("\xf8\xb1\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x8FFFE,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+8FFFF",
        (isASCII) ? "\xf2\x8f\xbf\xbf" : I8_to_native("\xf8\xb1\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x8FFFF,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+9FFFE",
        (isASCII) ? "\xf2\x9f\xbf\xbe" : I8_to_native("\xf8\xb3\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x9FFFE,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+9FFFF",
        (isASCII) ? "\xf2\x9f\xbf\xbf" : I8_to_native("\xf8\xb3\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x9FFFF,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+AFFFE",
        (isASCII) ? "\xf2\xaf\xbf\xbe" : I8_to_native("\xf8\xb5\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xAFFFE,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+AFFFF",
        (isASCII) ? "\xf2\xaf\xbf\xbf" : I8_to_native("\xf8\xb5\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xAFFFF,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+BFFFE",
        (isASCII) ? "\xf2\xbf\xbf\xbe" : I8_to_native("\xf8\xb7\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xBFFFE,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+BFFFF",
        (isASCII) ? "\xf2\xbf\xbf\xbf" : I8_to_native("\xf8\xb7\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xBFFFF,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+CFFFE",
        (isASCII) ? "\xf3\x8f\xbf\xbe" : I8_to_native("\xf8\xb9\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xCFFFE,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+CFFFF",
        (isASCII) ? "\xf3\x8f\xbf\xbf" : I8_to_native("\xf8\xb9\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xCFFFF,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+DFFFE",
        (isASCII) ? "\xf3\x9f\xbf\xbe" : I8_to_native("\xf8\xbb\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xDFFFE,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+DFFFF",
        (isASCII) ? "\xf3\x9f\xbf\xbf" : I8_to_native("\xf8\xbb\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xDFFFF,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+EFFFE",
        (isASCII) ? "\xf3\xaf\xbf\xbe" : I8_to_native("\xf8\xbd\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xEFFFE,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+EFFFF",
        (isASCII) ? "\xf3\xaf\xbf\xbf" : I8_to_native("\xf8\xbd\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xEFFFF,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+FFFFE",
        (isASCII) ? "\xf3\xbf\xbf\xbe" : I8_to_native("\xf8\xbf\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xFFFFE,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+FFFFF",
        (isASCII) ? "\xf3\xbf\xbf\xbf" : I8_to_native("\xf8\xbf\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0xFFFFF,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+10FFFE",
        (isASCII) ? "\xf4\x8f\xbf\xbe" : I8_to_native("\xf9\xa1\xbf\xbf\xbe"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x10FFFE,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "non-character code point U+10FFFF",
        (isASCII) ? "\xf4\x8f\xbf\xbf" : I8_to_native("\xf9\xa1\xbf\xbf\xbf"),
        $UTF8_WARN_NONCHAR, $UTF8_DISALLOW_NONCHAR,
        'nonchar', 0x10FFFF,
        (isASCII) ? 4 : 5,
        qr/Unicode non-character.*is not recommended for open interchange/
    ],
    [ "requires at least 32 bits",
        (isASCII)
         ? "\xfe\x82\x80\x80\x80\x80\x80"
         : I8_to_native("\xff\xa0\xa0\xa0\xa0\xa0\xa0\xa2\xa0\xa0\xa0\xa0\xa0\xa0"),
        # This code point is chosen so that it is representable in a UV on
        # 32-bit machines
        $UTF8_WARN_ABOVE_31_BIT, $UTF8_DISALLOW_ABOVE_31_BIT,
        'utf8', 0x80000000, (isASCII) ? 7 :14,
        qr/Code point 0x80000000 is not Unicode, and not portable/
    ],
    [ "requires at least 32 bits, and use SUPER-type flags, instead of ABOVE_31_BIT",
        (isASCII)
         ? "\xfe\x82\x80\x80\x80\x80\x80"
         : I8_to_native("\xff\xa0\xa0\xa0\xa0\xa0\xa0\xa2\xa0\xa0\xa0\xa0\xa0\xa0"),
        $UTF8_WARN_SUPER, $UTF8_DISALLOW_SUPER,
        'utf8', 0x80000000, (isASCII) ? 7 :14,
        qr/Code point 0x80000000 is not Unicode, and not portable/
    ],
    [ "overflow with warnings/disallow for more than 31 bits",
        # This tests the interaction of WARN_ABOVE_31_BIT/DISALLOW_ABOVE_31_BIT
        # with overflow.  The overflow malformation is never allowed, so
        # preventing it takes precedence if the ABOVE_31_BIT options would
        # otherwise allow in an overflowing value.  The ASCII code points (1
        # for 32-bits; 1 for 64) were chosen because the old overflow
        # detection algorithm did not catch them; this means this test also
        # checks for that fix.  The EBCDIC are arbitrary overflowing ones
        # since we have no reports of failures with it.
       (($is64bit)
        ? ((isASCII)
           ? "\xff\x80\x90\x90\x90\xbf\xbf\xbf\xbf\xbf\xbf\xbf\xbf"
           : I8_to_native("\xff\xB0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0\xa0"))
        : ((isASCII)
           ? "\xfe\x86\x80\x80\x80\x80\x80"
           : I8_to_native("\xff\xa0\xa0\xa0\xa0\xa0\xa0\xa4\xa0\xa0\xa0\xa0\xa0\xa0"))),

        # We include both warning categories to make sure the ABOVE_31_BIT one
        # has precedence
        "$UTF8_WARN_ABOVE_31_BIT|$UTF8_WARN_SUPER",
        "$UTF8_DISALLOW_ABOVE_31_BIT",
        'utf8', 0,
        (! isASCII) ? 14 : ($is64bit) ? 13 : 7,
        qr/overflow at byte .*, after start byte 0xf/
    ],
);

if ($is64bit) {
    no warnings qw{portable overflow};
    push @tests,
        [ "More than 32 bits",
            (isASCII)
            ? "\xff\x80\x80\x80\x80\x80\x81\x80\x80\x80\x80\x80\x80"
            : I8_to_native("\xff\xa0\xa0\xa0\xa0\xa0\xa2\xa0\xa0\xa0\xa0\xa0\xa0\xa0"),
            $UTF8_WARN_ABOVE_31_BIT, $UTF8_DISALLOW_ABOVE_31_BIT,
            'utf8', 0x1000000000, (isASCII) ? 13 : 14,
            qr/Code point 0x.* is not Unicode, and not portable/
        ];
}

foreach my $test (@tests) {
    my ($testname, $bytes, $warn_flags, $disallow_flags, $category, $allowed_uv, $expected_len, $message ) = @$test;

    my $length = length $bytes;
    my $will_overflow = $testname =~ /overflow/;

    # This is more complicated than the malformations tested earlier, as there
    # are several orthogonal variables involved.  We test all the subclasses
    # of utf8 warnings to verify they work with and without the utf8 class,
    # and don't have effects on other sublass warnings
    foreach my $warning ('utf8', 'surrogate', 'nonchar', 'non_unicode') {
        foreach my $warn_flag (0, $warn_flags) {
            foreach my $disallow_flag (0, $disallow_flags) {
                foreach my $do_warning (0, 1) {

                    my $eval_warn = $do_warning
                                  ? "use warnings '$warning'"
                                  : $warning eq "utf8"
                                    ? "no warnings 'utf8'"
                                    : "use warnings 'utf8'; no warnings '$warning'";

                    # is effectively disallowed if will overflow, even if the
                    # flag indicates it is allowed, fix up test name to
                    # indicate this as well
                    my $disallowed = $disallow_flag || $will_overflow;

                    my $this_name = "utf8n_to_uvchr() $testname: " . (($disallow_flag)
                                                    ? 'disallowed'
                                                    : ($disallowed)
                                                        ? 'ABOVE_31_BIT allowed'
                                                        : 'allowed');
                    $this_name .= ", $eval_warn";
                    $this_name .= ", " . (($warn_flag)
                                          ? 'with warning flag'
                                          : 'no warning flag');

                    undef @warnings;
                    my $ret_ref;
                    my $display_bytes = display_bytes($bytes);
                    my $call = "Call was: $eval_warn; \$ret_ref = test_utf8n_to_uvchr('$display_bytes', $length, $warn_flag|$disallow_flag)";
                    my $eval_text =      "$eval_warn; \$ret_ref = test_utf8n_to_uvchr('$bytes', $length, $warn_flag|$disallow_flag)";
                    eval "$eval_text";
                    if (! ok ("$@ eq ''", "$this_name: eval succeeded")) {
                        diag "\$!='$!'; eval'd=\"$call\"";
                        next;
                    }
                    if ($disallowed) {
                        unless (is($ret_ref->[0], 0, "$this_name: Returns 0"))
                        {
                            diag $call;
                        }
                    }
                    else {
                        unless (is($ret_ref->[0], $allowed_uv,
                                            "$this_name: Returns expected uv"))
                        {
                            diag $call;
                        }
                    }
                    unless (is($ret_ref->[1], $expected_len,
                                    "$this_name: Returns expected length"))
                    {
                        diag $call;
                    }

                    if (! $do_warning
                        && ($warning eq 'utf8' || $warning eq $category))
                    {
                        if (!is(scalar @warnings, 0,
                                            "$this_name: No warnings generated"))
                        {
                            diag $call;
                            diag "The warnings were: " . join(", ", @warnings);
                        }
                    }
                    elsif ($will_overflow
                           && ! $disallow_flag
                           && $warning eq 'utf8')
                    {

                        # Will get the overflow message instead of the expected
                        # message under these circumstances, as they would
                        # otherwise accept an overflowed value, which the code
                        # should not allow, so falls back to overflow.
                        if (is(scalar @warnings, 1,
                               "$this_name: Got a single warning "))
                        {
                            unless (like($warnings[0], qr/overflow/,
                                        "$this_name: Got overflow warning"))
                            {
                                diag $call;
                            }
                        }
                        else {
                            diag $call;
                            if (scalar @warnings) {
                                diag "The warnings were: "
                                                        . join(", ", @warnings);
                            }
                        }
                    }
                    elsif ($warn_flag
                           && ($warning eq 'utf8' || $warning eq $category))
                    {
                        if (is(scalar @warnings, 1,
                               "$this_name: Got a single warning "))
                        {
                            unless (like($warnings[0], $message,
                                        "$this_name: Got expected warning"))
                            {
                                diag $call;
                            }
                        }
                        else {
                            diag $call;
                            if (scalar @warnings) {
                                diag "The warnings were: "
                                                        . join(", ", @warnings);
                            }
                        }
                    }

                    # Check CHECK_ONLY results when the input is disallowed.  Do
                    # this when actually disallowed, not just when the
                    # $disallow_flag is set
                    if ($disallowed) {
                        undef @warnings;
                        $ret_ref = test_utf8n_to_uvchr($bytes, $length,
                                                $disallow_flag|$UTF8_CHECK_ONLY);
                        unless (is($ret_ref->[0], 0, "$this_name, CHECK_ONLY: Returns 0")) {
                            diag $call;
                        }
                        unless (is($ret_ref->[1], -1,
                            "$this_name: CHECK_ONLY: returns expected length"))
                        {
                            diag $call;
                        }
                        if (! is(scalar @warnings, 0,
                            "$this_name, CHECK_ONLY: no warnings generated"))
                        {
                            diag $call;
                            diag "The warnings were: " . join(", ", @warnings);
                        }
                    }

                    # Now repeat some of the above, but for
                    # uvchr_to_utf8_flags().  Since this comes from an
                    # existing code point, it hasn't overflowed.
                    next if $will_overflow;

                    # The warning and disallow flags passed in are for
                    # utf8n_to_uvchr().  Convert them for
                    # uvchr_to_utf8_flags().
                    my $uvchr_warn_flag = 0;
                    my $uvchr_disallow_flag = 0;
                    if ($warn_flag) {
                        if ($warn_flag == $UTF8_WARN_SURROGATE) {
                            $uvchr_warn_flag = $UNICODE_WARN_SURROGATE
                        }
                        elsif ($warn_flag == $UTF8_WARN_NONCHAR) {
                            $uvchr_warn_flag = $UNICODE_WARN_NONCHAR
                        }
                        elsif ($warn_flag == $UTF8_WARN_SUPER) {
                            $uvchr_warn_flag = $UNICODE_WARN_SUPER
                        }
                        elsif ($warn_flag == $UTF8_WARN_ABOVE_31_BIT) {
                            $uvchr_warn_flag = $UNICODE_WARN_ABOVE_31_BIT;
                        }
                        else {
                            fail(sprintf "Unexpected warn flag: %x",
                                 $warn_flag);
                            next;
                        }
                    }
                    if ($disallow_flag) {
                        if ($disallow_flag == $UTF8_DISALLOW_SURROGATE) {
                            $uvchr_disallow_flag = $UNICODE_DISALLOW_SURROGATE
                        }
                        elsif ($disallow_flag == $UTF8_DISALLOW_NONCHAR) {
                            $uvchr_disallow_flag = $UNICODE_DISALLOW_NONCHAR
                        }
                        elsif ($disallow_flag == $UTF8_DISALLOW_SUPER) {
                            $uvchr_disallow_flag = $UNICODE_DISALLOW_SUPER
                        }
                        elsif ($disallow_flag == $UTF8_DISALLOW_ABOVE_31_BIT) {
                            $uvchr_disallow_flag =
                            $UNICODE_DISALLOW_ABOVE_31_BIT;
                        }
                        else {
                            fail(sprintf "Unexpected disallow flag: %x",
                                 $disallow_flag);
                            next;
                        }
                    }

                    $disallowed = $uvchr_disallow_flag;

                    $this_name = "uvchr_to_utf8_flags() $testname: "
                                                  . (($uvchr_disallow_flag)
                                                    ? 'disallowed'
                                                    : ($disallowed)
                                                      ? 'ABOVE_31_BIT allowed'
                                                      : 'allowed');
                    $this_name .= ", $eval_warn";
                    $this_name .= ", " . (($uvchr_warn_flag)
                                          ? 'with warning flag'
                                          : 'no warning flag');

                    undef @warnings;
                    my $ret;
                    my $warn_flag = sprintf "0x%x", $uvchr_warn_flag;
                    my $disallow_flag = sprintf "0x%x", $uvchr_disallow_flag;
                    $call = sprintf "call was: $eval_warn; \$ret = test_uvchr_to_utf8_flags(0x%x, $warn_flag|$disallow_flag)", $allowed_uv;
                    $eval_text = "$eval_warn; \$ret = test_uvchr_to_utf8_flags($allowed_uv, $warn_flag|$disallow_flag)";
                    eval "$eval_text";
                    if (! ok ("$@ eq ''", "$this_name: eval succeeded")) {
                        diag "\$!='$!'; eval'd=\"$eval_text\"";
                        next;
                    }
                    if ($disallowed) {
                        unless (is($ret, undef, "$this_name: Returns undef")) {
                            diag $call;
                        }
                    }
                    else {
                        unless (is($ret, $bytes, "$this_name: Returns expected string")) {
                            diag $call;
                        }
                    }
                    if (! $do_warning
                        && ($warning eq 'utf8' || $warning eq $category))
                    {
                        if (!is(scalar @warnings, 0,
                                            "$this_name: No warnings generated"))
                        {
                            diag $call;
                            diag "The warnings were: " . join(", ", @warnings);
                        }
                    }
                    elsif ($uvchr_warn_flag
                           && ($warning eq 'utf8' || $warning eq $category))
                    {
                        if (is(scalar @warnings, 1,
                               "$this_name: Got a single warning "))
                        {
                            unless (like($warnings[0], $message,
                                            "$this_name: Got expected warning"))
                            {
                                diag $call;
                            }
                        }
                        else {
                            diag $call;
                            if (scalar @warnings) {
                                diag "The warnings were: "
                                                        . join(", ", @warnings);
                            }
                        }
                    }
                }
            }
        }
    }
}

done_testing;
