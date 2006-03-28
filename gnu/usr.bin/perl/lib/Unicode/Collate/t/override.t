BEGIN {
    unless ("A" eq pack('U', 0x41)) {
	print "1..0 # Unicode::Collate " .
	    "cannot stringify a Unicode code point\n";
	exit 0;
    }
    if ($ENV{PERL_CORE}) {
	chdir('t') if -d 't';
	@INC = $^O eq 'MacOS' ? qw(::lib) : qw(../lib);
    }
}

use Test;
BEGIN { plan tests => 76 };

use strict;
use warnings;
use Unicode::Collate;

ok(1);

##### 2..6

my $all_undef_8 = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  overrideCJK => undef,
  overrideHangul => undef,
  UCA_Version => 8,
);

# All in the Unicode code point order.
# No hangul decomposition.

ok($all_undef_8->lt("\x{3402}", "\x{4E00}"));
ok($all_undef_8->lt("\x{4DFF}", "\x{4E00}"));
ok($all_undef_8->lt("\x{4E00}", "\x{AC00}"));
ok($all_undef_8->gt("\x{AC00}", "\x{1100}\x{1161}"));
ok($all_undef_8->gt("\x{AC00}", "\x{ABFF}"));


##### 7..11

my $all_undef_9 = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  overrideCJK => undef,
  overrideHangul => undef,
  UCA_Version => 9,
);

# CJK Ideo. < CJK ext A/B < Others.
# No hangul decomposition.

ok($all_undef_9->lt("\x{4E00}", "\x{3402}"));
ok($all_undef_9->lt("\x{3402}", "\x{20000}"));
ok($all_undef_9->lt("\x{20000}", "\x{AC00}"));
ok($all_undef_9->gt("\x{AC00}", "\x{1100}\x{1161}"));
ok($all_undef_9->gt("\x{AC00}", "\x{ABFF}")); # U+ABFF: not assigned

##### 12..16

my $ignoreHangul = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  overrideHangul => sub {()},
  entry => <<'ENTRIES',
AE00 ; [.0100.0020.0002.AE00]  # Hangul GEUL
ENTRIES
);

# All Hangul Syllables except U+AE00 are ignored.

ok($ignoreHangul->eq("\x{AC00}", ""));
ok($ignoreHangul->lt("\x{AC00}", "\0"));
ok($ignoreHangul->lt("\x{AC00}", "\x{AE00}"));
ok($ignoreHangul->lt("\x{AC00}", "\x{1100}\x{1161}")); # Jamo are not ignored.
ok($ignoreHangul->lt("Pe\x{AE00}rl", "Perl")); # 'r' is unassigned.


my $ignoreCJK = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  overrideCJK => sub {()},
  entry => <<'ENTRIES',
5B57 ; [.0107.0020.0002.5B57]  # CJK Ideograph "Letter"
ENTRIES
);

# All CJK Unified Ideographs except U+5B57 are ignored.

##### 17..21
ok($ignoreCJK->eq("\x{4E00}", ""));
ok($ignoreCJK->lt("\x{4E00}", "\0"));
ok($ignoreCJK->eq("Pe\x{4E00}rl", "Perl")); # U+4E00 is a CJK.
ok($ignoreCJK->gt("\x{4DFF}", "\x{4E00}")); # U+4DFF is not CJK.
ok($ignoreCJK->lt("Pe\x{5B57}rl", "Perl")); # 'r' is unassigned.

##### 22..29
ok($ignoreCJK->eq("\x{3400}", ""));
ok($ignoreCJK->eq("\x{4DB5}", ""));
ok($ignoreCJK->eq("\x{9FA5}", ""));
ok($ignoreCJK->eq("\x{9FA6}", "")); # UI since Unicode 4.1.0
ok($ignoreCJK->eq("\x{9FBB}", "")); # UI since Unicode 4.1.0
ok($ignoreCJK->gt("\x{9FBC}", "Perl"));
ok($ignoreCJK->eq("\x{20000}", ""));
ok($ignoreCJK->eq("\x{2A6D6}", ""));

##### 30..37
$ignoreCJK->change(UCA_Version => 9);
ok($ignoreCJK->eq("\x{3400}", ""));
ok($ignoreCJK->eq("\x{4DB5}", ""));
ok($ignoreCJK->eq("\x{9FA5}", ""));
ok($ignoreCJK->gt("\x{9FA6}", "Perl"));
ok($ignoreCJK->gt("\x{9FBB}", "Perl"));
ok($ignoreCJK->gt("\x{9FBC}", "Perl"));
ok($ignoreCJK->eq("\x{20000}", ""));
ok($ignoreCJK->eq("\x{2A6D6}", ""));

##### 38..45
$ignoreCJK->change(UCA_Version => 8);
ok($ignoreCJK->eq("\x{3400}", ""));
ok($ignoreCJK->eq("\x{4DB5}", ""));
ok($ignoreCJK->eq("\x{9FA5}", ""));
ok($ignoreCJK->gt("\x{9FA6}", "Perl"));
ok($ignoreCJK->gt("\x{9FBB}", "Perl"));
ok($ignoreCJK->gt("\x{9FBC}", "Perl"));
ok($ignoreCJK->eq("\x{20000}", ""));
ok($ignoreCJK->eq("\x{2A6D6}", ""));

##### 46..53
$ignoreCJK->change(UCA_Version => 14);
ok($ignoreCJK->eq("\x{3400}", ""));
ok($ignoreCJK->eq("\x{4DB5}", ""));
ok($ignoreCJK->eq("\x{9FA5}", ""));
ok($ignoreCJK->eq("\x{9FA6}", "")); # UI since Unicode 4.1.0
ok($ignoreCJK->eq("\x{9FBB}", "")); # UI since Unicode 4.1.0
ok($ignoreCJK->gt("\x{9FBC}", "Perl"));
ok($ignoreCJK->eq("\x{20000}", ""));
ok($ignoreCJK->eq("\x{2A6D6}", ""));

##### 54..76
my $overCJK = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  entry => <<'ENTRIES',
0061 ; [.0101.0020.0002.0061] # latin a
0041 ; [.0101.0020.0008.0041] # LATIN A
4E00 ; [.B1FC.0030.0004.4E00] # Ideograph; B1FC = FFFF - 4E03.
ENTRIES
  overrideCJK => sub {
    my $u = 0xFFFF - $_[0]; # reversed
    [$u, 0x20, 0x2, $u];
  },
);

ok($overCJK->lt("a", "A")); # diff. at level 3.
ok($overCJK->lt( "\x{4E03}",  "\x{4E00}")); # diff. at level 2.
ok($overCJK->lt("A\x{4E03}", "A\x{4E00}"));
ok($overCJK->lt("A\x{4E03}", "a\x{4E00}"));
ok($overCJK->lt("a\x{4E03}", "A\x{4E00}"));

ok($overCJK->gt("a\x{3400}", "A\x{4DB5}"));
ok($overCJK->gt("a\x{4DB5}", "A\x{9FA5}"));
ok($overCJK->gt("a\x{9FA5}", "A\x{9FA6}"));
ok($overCJK->gt("a\x{9FA6}", "A\x{9FBB}"));
ok($overCJK->lt("a\x{9FBB}", "A\x{9FBC}"));
ok($overCJK->lt("a\x{9FBC}", "A\x{9FBF}"));

$overCJK->change(UCA_Version => 9);

ok($overCJK->gt("a\x{3400}", "A\x{4DB5}"));
ok($overCJK->gt("a\x{4DB5}", "A\x{9FA5}"));
ok($overCJK->lt("a\x{9FA5}", "A\x{9FA6}"));
ok($overCJK->lt("a\x{9FA6}", "A\x{9FBB}"));
ok($overCJK->lt("a\x{9FBB}", "A\x{9FBC}"));
ok($overCJK->lt("a\x{9FBC}", "A\x{9FBF}"));

$overCJK->change(UCA_Version => 14);

ok($overCJK->gt("a\x{3400}", "A\x{4DB5}"));
ok($overCJK->gt("a\x{4DB5}", "A\x{9FA5}"));
ok($overCJK->gt("a\x{9FA5}", "A\x{9FA6}"));
ok($overCJK->gt("a\x{9FA6}", "A\x{9FBB}"));
ok($overCJK->lt("a\x{9FBB}", "A\x{9FBC}"));
ok($overCJK->lt("a\x{9FBC}", "A\x{9FBF}"));

