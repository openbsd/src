
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

use strict;
use warnings;
BEGIN { $| = 1; print "1..35\n"; }
my $count = 0;
sub ok ($;$) {
    my $p = my $r = shift;
    if (@_) {
	my $x = shift;
	$p = !defined $x ? !defined $r : !defined $r ? 0 : $r eq $x;
    }
    print $p ? "ok" : "not ok", ' ', ++$count, "\n";
}

use Unicode::Collate;

ok(1);

#########################

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

##### 17..21

my $undefHangul = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  overrideHangul => sub {
    my $u = shift;
    return $u == 0xAE00 ? 0x100 : undef;
  }
);

# All Hangul Syllables except U+AE00 are undefined.

ok($undefHangul->lt("\x{AE00}", "r"));
ok($undefHangul->gt("\x{AC00}", "r"));
ok($undefHangul->gt("\x{AC00}", "\x{1100}\x{1161}"));
ok($undefHangul->lt("Pe\x{AE00}rl", "Perl")); # 'r' is unassigned.
ok($undefHangul->lt("\x{AC00}", "\x{B000}"));

##### 22..25

my $undefCJK = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  overrideCJK => sub {
    my $u = shift;
    return $u == 0x4E00 ? 0x100 : undef;
  }
);

# All CJK Ideographs except U+4E00 are undefined.

ok($undefCJK->lt("\x{4E00}", "r"));
ok($undefCJK->lt("\x{5000}", "r")); # still CJK < unassigned
ok($undefCJK->lt("Pe\x{4E00}rl", "Perl")); # 'r' is unassigned.
ok($undefCJK->lt("\x{5000}", "\x{6000}"));

##### 26..30

my $cpHangul = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  overrideHangul => sub { shift }
);

ok($cpHangul->lt("\x{AC00}", "\x{AC01}"));
ok($cpHangul->lt("\x{AC01}", "\x{D7A3}"));
ok($cpHangul->lt("\x{D7A3}", "r")); # 'r' is unassigned.
ok($cpHangul->lt("r", "\x{D7A4}"));
ok($cpHangul->lt("\x{D7A3}", "\x{4E00}"));

##### 31..35

my $arrayHangul = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  overrideHangul => sub {
    my $u = shift;
    return [$u, 0x20, 0x2, $u];
  }
);

ok($arrayHangul->lt("\x{AC00}", "\x{AC01}"));
ok($arrayHangul->lt("\x{AC01}", "\x{D7A3}"));
ok($arrayHangul->lt("\x{D7A3}", "r")); # 'r' is unassigned.
ok($arrayHangul->lt("r", "\x{D7A4}"));
ok($arrayHangul->lt("\x{D7A3}", "\x{4E00}"));

