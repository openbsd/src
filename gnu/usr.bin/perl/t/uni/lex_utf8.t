#!./perl -w
#
# This script is written intentionally in UTF-8

BEGIN {
    $| = 1;

    chdir 't';
    @INC = '../lib';
    require './test.pl';
    skip_all_if_miniperl("no dynamic loading on miniperl, no re");
    skip_all('EBCDIC') if $::IS_EBCDIC;
}

use strict;

plan (tests => 15);
use charnames ':full';

use utf8;

my $A_with_ogonek = "Ą";
my $micro_sign = "µ";
my $hex_first = "a\x{A2}Ą";
my $hex_last = "aĄ\x{A2}";
my $name_first = "b\N{MICRO SIGN}Ɓ";
my $name_last = "bƁ\N{MICRO SIGN}";
my $uname_first = "b\N{U+00B5}Ɓ";
my $uname_last = "bƁ\N{U+00B5}";
my $octal_first = "c\377Ć";
my $octal_last = "cĆ\377";

do {
	use bytes;
	is((join "", unpack("C*", $A_with_ogonek)), "196" . "132", 'single char above 0x100');
	is((join "", unpack("C*", $micro_sign)), "194" . "181", 'single char in 0x80 .. 0xFF');
	is((join "", unpack("C*", $hex_first)), "97" . "194" . "162" . "196" . "132", 'a . \x{A2} . char above 0x100');
	is((join "", unpack("C*", $hex_last)), "97" . "196" . "132" . "194" . "162", 'a . char above 0x100 . \x{A2}');
	is((join "", unpack("C*", $name_first)), "98" . "194" . "181" . "198" . "129", 'b . \N{MICRO SIGN} . char above 0x100');
	is((join "", unpack("C*", $name_last)), "98" . "198" . "129" . "194" . "181", 'b . char above 0x100 . \N{MICRO SIGN}');
	is((join "", unpack("C*", $uname_first)), "98" . "194" . "181" . "198" . "129", 'b . \N{U+00B5} . char above 0x100');
	is((join "", unpack("C*", $uname_last)), "98" . "198" . "129" . "194" . "181", 'b . char above 0x100 . \N{U+00B5}');
	is((join "", unpack("C*", $octal_first)), "99" . "195" . "191" . "196" . "134", 'c . \377 . char above 0x100');
	is((join "", unpack("C*", $octal_last)), "99" . "196" . "134" . "195" . "191", 'c . char above 0x100 . \377');
};

{
    local $SIG{__WARN__} = sub {};
    eval "our $::\xe9; $\xe9";
    unlike $@, qr/utf8_heavy/,
	'No utf8_heavy errors with our() syntax errors';
}

# [perl #120463]
$_ = "a";
eval 's αaαbα';
is $@, "", 's/// compiles, where / is actually a wide character';
is $_, "b", 'substitution worked';
$_ = "a";
eval 'tr νaνbν';
is $@, "", 'y/// compiles, where / is actually a wide character';
is $_, "b", 'transliteration worked';

__END__

