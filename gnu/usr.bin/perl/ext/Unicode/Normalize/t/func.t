# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

BEGIN {
    if (ord("A") == 193) {
	print "1..0 # Unicode::Normalize not ported to EBCDIC\n";
	exit 0;
    }
}

#########################

use Test;
use strict;
use warnings;
BEGIN { plan tests => 13 };
use Unicode::Normalize qw(:all);
ok(1); # If we made it this far, we're ok.

#########################

print getCombinClass(   0) == 0
   && getCombinClass( 768) == 230
   && getCombinClass(1809) == 36
   && ($] < 5.007003 || getCombinClass(0x1D167) == 1) # Unicode 3.1
  ? "ok" : "not ok", " 2\n";

print ! defined getCanon( 0)
   && ! defined getCanon(41)
   && getCanon(0x00C0) eq pack('U*', 0x0041, 0x0300)
   && getCanon(0x00EF) eq pack('U*', 0x0069, 0x0308)
   && getCanon(0x304C) eq pack('U*', 0x304B, 0x3099)
   && getCanon(0x1EA4) eq pack('U*', 0x0041, 0x0302, 0x0301)
   && getCanon(0x1F82) eq "\x{03B1}\x{0313}\x{0300}\x{0345}"
   && getCanon(0x1FAF) eq pack('U*', 0x03A9, 0x0314, 0x0342, 0x0345)
   && getCanon(0xAC00) eq pack('U*', 0x1100, 0x1161)
   && getCanon(0xAE00) eq pack('U*', 0x1100, 0x1173, 0x11AF)
   && ! defined getCanon(0x212C)
   && ! defined getCanon(0x3243)
   && getCanon(0xFA2D) eq pack('U*', 0x9DB4)
  ? "ok" : "not ok", " 3\n";

print ! defined getCompat( 0)
   && ! defined getCompat(41)
   && getCompat(0x00C0) eq pack('U*', 0x0041, 0x0300)
   && getCompat(0x00EF) eq pack('U*', 0x0069, 0x0308)
   && getCompat(0x304C) eq pack('U*', 0x304B, 0x3099)
   && getCompat(0x1EA4) eq pack('U*', 0x0041, 0x0302, 0x0301)
   && getCompat(0x1F82) eq pack('U*', 0x03B1, 0x0313, 0x0300, 0x0345)
   && getCompat(0x1FAF) eq pack('U*', 0x03A9, 0x0314, 0x0342, 0x0345)
   && getCompat(0x212C) eq pack('U*', 0x0042)
   && getCompat(0x3243) eq pack('U*', 0x0028, 0x81F3, 0x0029)
   && getCompat(0xAC00) eq pack('U*', 0x1100, 0x1161)
   && getCompat(0xAE00) eq pack('U*', 0x1100, 0x1173, 0x11AF)
   && getCompat(0xFA2D) eq pack('U*', 0x9DB4)
  ? "ok" : "not ok", " 4\n";

print ! defined getComposite( 0,  0)
   && ! defined getComposite( 0, 41)
   && ! defined getComposite(41,  0)
   && ! defined getComposite(41, 41)
   && ! defined getComposite(12, 0x0300)
   && ! defined getComposite(0x0055, 0xFF00)
   && 0x00C0 == getComposite(0x0041, 0x0300)
   && 0x00D9 == getComposite(0x0055, 0x0300)
   && 0x1E14 == getComposite(0x0112, 0x0300)
   && 0xAC00 == getComposite(0x1100, 0x1161)
   && 0xADF8 == getComposite(0x1100, 0x1173)
   && ! defined getComposite(0x1100, 0x11AF)
   && ! defined getComposite(0x1173, 0x11AF)
   && ! defined getComposite(0xAC00, 0x11A7)
   && 0xAC01 == getComposite(0xAC00, 0x11A8)
   && 0xAE00 == getComposite(0xADF8, 0x11AF)
  ? "ok" : "not ok", " 5\n";

print ! isExclusion( 0)
   && ! isExclusion(41)
   && isExclusion(2392)  # DEVANAGARI LETTER QA
   && isExclusion(3907)  # TIBETAN LETTER GHA
   && isExclusion(64334) # HEBREW LETTER PE WITH RAFE
  ? "ok" : "not ok", " 6\n";

print ! isSingleton( 0)
   && isSingleton(0x212B) # ANGSTROM SIGN
  ? "ok" : "not ok", " 7\n";

print reorder("") eq ""
   && reorder(pack("U*", 0x0041, 0x0300, 0x0315, 0x0313, 0x031b, 0x0061))
      eq pack("U*", 0x0041, 0x031b, 0x0300, 0x0313, 0x0315, 0x0061)
   && reorder(pack("U*", 0x00C1, 0x0300, 0x0315, 0x0313, 0x031b,
	0x0061, 0x309A, 0x3099))
      eq pack("U*", 0x00C1, 0x031b, 0x0300, 0x0313, 0x0315,
	0x0061, 0x309A, 0x3099)
  ? "ok" : "not ok", " 8\n";

sub answer { defined $_[0] ? $_[0] ? "YES" : "NO" : "MAYBE" }

print answer(checkNFD(""))  eq "YES"
  &&  answer(checkNFC(""))  eq "YES"
  &&  answer(checkNFKD("")) eq "YES"
  &&  answer(checkNFKC("")) eq "YES"
  &&  answer(check("NFD", "")) eq "YES"
  &&  answer(check("NFC", "")) eq "YES"
  &&  answer(check("NFKD","")) eq "YES"
  &&  answer(check("NFKC","")) eq "YES"
# U+0000 to U+007F are prenormalized in all the normalization forms.
  && answer(checkNFD("AZaz\t12!#`"))  eq "YES"
  && answer(checkNFC("AZaz\t12!#`"))  eq "YES"
  && answer(checkNFKD("AZaz\t12!#`")) eq "YES"
  && answer(checkNFKC("AZaz\t12!#`")) eq "YES"
  && answer(check("D", "AZaz\t12!#`")) eq "YES"
  && answer(check("C", "AZaz\t12!#`")) eq "YES"
  && answer(check("KD","AZaz\t12!#`")) eq "YES"
  && answer(check("KC","AZaz\t12!#`")) eq "YES"
  ? "ok" : "not ok", " 9\n";

print 1
  && answer(checkNFD(NFD(pack('U*', 0xC1, 0x1100, 0x1173, 0x11AF)))) eq "YES"
  && answer(checkNFD(pack('U*', 0x20, 0xC1, 0x1100, 0x1173, 0x11AF))) eq "NO"
  && answer(checkNFC(pack('U*', 0x20, 0xC1, 0x1173, 0x11AF))) eq "MAYBE"
  && answer(checkNFC(pack('U*', 0x20, 0xC1, 0xAE00, 0x1100))) eq "YES"
  && answer(checkNFC(pack('U*', 0x20, 0xC1, 0xAE00, 0x1100, 0x300))) eq "MAYBE"
  && answer(checkNFC(pack('U*', 0x20, 0xC1, 0xFF71, 0x2025))) eq "YES"
  && answer(check("NFC", pack('U*', 0x20, 0xC1, 0x212B, 0x300))) eq "NO"
  && answer(checkNFKD(pack('U*', 0x20, 0xC1, 0xFF71, 0x2025))) eq "NO"
  && answer(checkNFKC(pack('U*', 0x20, 0xC1, 0xAE00, 0x2025))) eq "NO"
  ? "ok" : "not ok", " 10\n";

"012ABC" =~ /(\d+)(\w+)/;
print "012" eq NFC $1 && "ABC" eq NFC $2
  ? "ok" : "not ok", " 11\n";

print "012" eq normalize('C', $1) && "ABC" eq normalize('C', $2)
  ? "ok" : "not ok", " 12\n";

print "012" eq normalize('NFC', $1) && "ABC" eq normalize('NFC', $2)
  ? "ok" : "not ok", " 13\n";
 # s/^NF// in normalize() must not prevent using $1, $&, etc.

