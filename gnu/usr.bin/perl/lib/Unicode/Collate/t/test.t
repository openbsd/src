
BEGIN {
    unless ("A" eq pack('U', 0x41)) {
	print "1..0 # Unicode::Collate " .
	    "cannot stringify a Unicode code point\n";
	exit 0;
    }
}

BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir('t') if -d 't';
        @INC = $^O eq 'MacOS' ? qw(::lib) : qw(../lib);
    }
}

use Test;
BEGIN { plan tests => 160 };

use strict;
use warnings;
use Unicode::Collate;

ok(1);

##### 2..6

my $Collator = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
);

ok(ref $Collator, "Unicode::Collate");


ok(
  join(':', $Collator->sort( 
    qw/ lib strict Carp ExtUtils CGI Time warnings Math overload Pod CPAN /
  ) ),
  join(':',
    qw/ Carp CGI CPAN ExtUtils lib Math overload Pod strict Time warnings /
  ),
);

ok($Collator->cmp("", ""), 0);
ok($Collator->eq("", ""));
ok($Collator->cmp("", "perl"), -1);

##### 7..17

sub _pack_U   { Unicode::Collate::pack_U(@_) }
sub _unpack_U { Unicode::Collate::unpack_U(@_) }

my $A_acute = _pack_U(0xC1);
my $a_acute = _pack_U(0xE1);
my $acute   = _pack_U(0x0301);

ok($Collator->cmp("A$acute", $A_acute), 0); # @version 3.1.1 (prev: -1)
ok($Collator->cmp($a_acute, $A_acute), -1);
ok($Collator->eq("A\cA$acute", $A_acute)); # UCA v9. \cA is invariant.

my %old_level = $Collator->change(level => 1);
ok($Collator->eq("A$acute", $A_acute));
ok($Collator->eq("A", $A_acute));

ok($Collator->change(level => 2)->eq($a_acute, $A_acute));
ok($Collator->lt("A", $A_acute));

ok($Collator->change(%old_level)->lt("A", $A_acute));
ok($Collator->lt("A", $A_acute));
ok($Collator->lt("A", $a_acute));
ok($Collator->lt($a_acute, $A_acute));

##### 18..20

eval { require Unicode::Normalize };
if (!$@) {
  my $NFD = Unicode::Collate->new(
    table => 'keys.txt',
    level => 1,
    entry => <<'ENTRIES',
0430  ; [.0CB5.0020.0002.0430] # CYRILLIC SMALL LETTER A
0410  ; [.0CB5.0020.0008.0410] # CYRILLIC CAPITAL LETTER A
04D3  ; [.0CBD.0020.0002.04D3] # CYRILLIC SMALL LETTER A WITH DIAERESIS
0430 0308 ; [.0CBD.0020.0002.04D3] # CYRILLIC SMALL LETTER A WITH DIAERESIS
04D2  ; [.0CBD.0020.0008.04D2] # CYRILLIC CAPITAL LETTER A WITH DIAERESIS
0410 0308 ; [.0CBD.0020.0008.04D2] # CYRILLIC CAPITAL LETTER A WITH DIAERESIS
0430 3099 ; [.0CBE.0020.0002.04D3] # A WITH KATAKANA VOICED
0430 3099 0308 ; [.0CBF.0020.0002.04D3] # A WITH KATAKANA VOICED, DIAERESIS
ENTRIES
  );
  ok($NFD->eq("\x{4D3}\x{325}", "\x{430}\x{308}\x{325}"));
  ok($NFD->lt("\x{430}\x{308}A", "\x{430}\x{308}B"));
  ok($NFD->lt("\x{430}\x{3099}B", "\x{430}\x{308}\x{3099}A"));
}
else {
  ok(1);
  ok(1);
  ok(1);
}

##### 21..34

my $trad = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
  ignoreName => qr/HANGUL|HIRAGANA|KATAKANA|BOPOMOFO/,
  level => 3,
  entry => << 'ENTRIES',
 0063 0068 ; [.0A3F.0020.0002.0063] % "ch" in traditional Spanish
 0043 0068 ; [.0A3F.0020.0008.0043] # "Ch" in traditional Spanish
ENTRIES
);
# 0063  ; [.0A3D.0020.0002.0063] # LATIN SMALL LETTER C
# 0064  ; [.0A49.0020.0002.0064] # LATIN SMALL LETTER D
# Deutsch sz is included in 'keys.txt';

ok(
  join(':', $trad->sort( qw/ acha aca ada acia acka / ) ),
  join(':',              qw/ aca acia acka acha ada / ),
);

ok(
  join(':', $Collator->sort( qw/ acha aca ada acia acka / ) ),
  join(':',                  qw/ aca acha acia acka ada / ),
);

ok($trad->eq("ocho", "oc\cAho")); # UCA v9
ok($trad->eq("ocho", "oc\0\cA\0\cBho")); # UCA v9
ok($trad->eq("-", ""));
ok($trad->gt("ocho", "oc-ho"));

$trad->change(UCA_Version => 8);

ok($trad->gt("ocho", "oc\cAho"));
ok($trad->gt("ocho", "oc\0\cA\0\cBho"));
ok($trad->eq("-", ""));
ok($trad->gt("ocho", "oc-ho"));

$trad->change(UCA_Version => 9);

my $hiragana = "\x{3042}\x{3044}";
my $katakana = "\x{30A2}\x{30A4}";

# HIRAGANA and KATAKANA are ignorable via ignoreName
ok($trad->eq($hiragana, ""));
ok($trad->eq("", $katakana));
ok($trad->eq($hiragana, $katakana));
ok($trad->eq($katakana, $hiragana));

##### 35..41

$Collator->change(level => 2);

ok($Collator->{level}, 2);

ok( $Collator->cmp("ABC","abc"), 0);
ok( $Collator->eq("ABC","abc") );
ok( $Collator->le("ABC","abc") );
ok( $Collator->cmp($hiragana, $katakana), 0);
ok( $Collator->eq($hiragana, $katakana) );
ok( $Collator->ge($hiragana, $katakana) );

##### 42..47

# hangul
ok( $Collator->eq("a\x{AC00}b", "a\x{1100}\x{1161}b") );
ok( $Collator->eq("a\x{AE00}b", "a\x{1100}\x{1173}\x{11AF}b") );
ok( $Collator->gt("a\x{AE00}b", "a\x{1100}\x{1173}b\x{11AF}") );
ok( $Collator->lt("a\x{AC00}b", "a\x{AE00}b") );
ok( $Collator->gt("a\x{D7A3}b", "a\x{C544}b") );
ok( $Collator->lt("a\x{C544}b", "a\x{30A2}b") ); # hangul < hiragana

##### 48..56

$Collator->change(%old_level, katakana_before_hiragana => 1);

ok($Collator->{level}, 4);

ok( $Collator->cmp("abc", "ABC"), -1);
ok( $Collator->ne("abc", "ABC") );
ok( $Collator->lt("abc", "ABC") );
ok( $Collator->le("abc", "ABC") );
ok( $Collator->cmp($hiragana, $katakana), 1);
ok( $Collator->ne($hiragana, $katakana) );
ok( $Collator->gt($hiragana, $katakana) );
ok( $Collator->ge($hiragana, $katakana) );

##### 57..62

$Collator->change(upper_before_lower => 1);

ok( $Collator->cmp("abc", "ABC"), 1);
ok( $Collator->ge("abc", "ABC"), 1);
ok( $Collator->gt("abc", "ABC"), 1);
ok( $Collator->cmp($hiragana, $katakana), 1);
ok( $Collator->ge($hiragana, $katakana), 1);
ok( $Collator->gt($hiragana, $katakana), 1);

##### 63..68

$Collator->change(katakana_before_hiragana => 0);

ok( $Collator->cmp("abc", "ABC"), 1);
ok( $Collator->cmp($hiragana, $katakana), -1);

$Collator->change(upper_before_lower => 0);

ok( $Collator->cmp("abc", "ABC"), -1);
ok( $Collator->le("abc", "ABC") );
ok( $Collator->cmp($hiragana, $katakana), -1);
ok( $Collator->lt($hiragana, $katakana) );

##### 69..70

my $ignoreAE = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
  ignoreChar => qr/^[aAeE]$/,
);

ok($ignoreAE->eq("element","lament"));
ok($ignoreAE->eq("Perl","ePrl"));

##### 71

my $onlyABC = Unicode::Collate->new(
    table => undef,
    normalization => undef,
    entry => << 'ENTRIES',
0061 ; [.0101.0020.0002.0061] # LATIN SMALL LETTER A
0041 ; [.0101.0020.0008.0041] # LATIN CAPITAL LETTER A
0062 ; [.0102.0020.0002.0062] # LATIN SMALL LETTER B
0042 ; [.0102.0020.0008.0042] # LATIN CAPITAL LETTER B
0063 ; [.0103.0020.0002.0063] # LATIN SMALL LETTER C
0043 ; [.0103.0020.0008.0043] # LATIN CAPITAL LETTER C
ENTRIES
);

ok(
  join(':', $onlyABC->sort( qw/ ABA BAC cc A Ab cAc aB / ) ),
  join(':',                 qw/ A aB Ab ABA BAC cAc cc / ),
);

##### 72..75

my $undefAE = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
  undefChar => qr/^[aAeE]$/,
);

ok($undefAE ->gt("edge","fog"));
ok($Collator->lt("edge","fog"));
ok($undefAE ->gt("lake","like"));
ok($Collator->lt("lake","like"));

##### 76..85

# Table is undefined, then no entry is defined.

my $undef_table = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  level => 1,
);

# in the Unicode code point order
ok($undef_table->lt('', 'A'));
ok($undef_table->lt('ABC', 'B'));

# Hangul should be decomposed (even w/o Unicode::Normalize).

ok($undef_table->lt("Perl", "\x{AC00}"));
ok($undef_table->eq("\x{AC00}", "\x{1100}\x{1161}"));
ok($undef_table->eq("\x{AE00}", "\x{1100}\x{1173}\x{11AF}"));
ok($undef_table->lt("\x{AE00}", "\x{3042}"));
  # U+AC00: Hangul GA
  # U+AE00: Hangul GEUL
  # U+3042: Hiragana A

# Weight for CJK Ideographs is defined, though.

ok($undef_table->lt("", "\x{4E00}"));
ok($undef_table->lt("\x{4E8C}","ABC"));
ok($undef_table->lt("\x{4E00}","\x{3042}"));
ok($undef_table->lt("\x{4E00}","\x{4E8C}"));
  # U+4E00: Ideograph "ONE"
  # U+4E8C: Ideograph "TWO"


##### 86..90

my $few_entries = Unicode::Collate->new(
  entry => <<'ENTRIES',
0050 ; [.0101.0020.0002.0050]  # P
0045 ; [.0102.0020.0002.0045]  # E
0052 ; [.0103.0020.0002.0052]  # R
004C ; [.0104.0020.0002.004C]  # L
1100 ; [.0105.0020.0002.1100]  # Hangul Jamo initial G
1175 ; [.0106.0020.0002.1175]  # Hangul Jamo middle I
5B57 ; [.0107.0020.0002.5B57]  # CJK Ideograph "Letter"
ENTRIES
  table => undef,
  normalization => undef,
);

# defined before undefined

my $sortABC = join '',
    $few_entries->sort(split //, "ABCDEFGHIJKLMNOPQRSTUVWXYZ ");

ok($sortABC eq "PERL ABCDFGHIJKMNOQSTUVWXYZ");

ok($few_entries->lt('E', 'D'));
ok($few_entries->lt("\x{5B57}", "\x{4E00}"));
ok($few_entries->lt("\x{AE30}", "\x{AC00}"));

# Hangul must be decomposed.

ok($few_entries->eq("\x{AC00}", "\x{1100}\x{1161}"));

##### 91..95

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

##### 96..100

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

##### 101..105

my $ignoreCJK = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  overrideCJK => sub {()},
  entry => <<'ENTRIES',
5B57 ; [.0107.0020.0002.5B57]  # CJK Ideograph "Letter"
ENTRIES
);

# All CJK Unified Ideographs except U+5B57 are ignored.

ok($ignoreCJK->eq("\x{4E00}", ""));
ok($ignoreCJK->lt("\x{4E00}", "\0"));
ok($ignoreCJK->eq("Pe\x{4E00}rl", "Perl")); # U+4E00 is a CJK.
ok($ignoreCJK->gt("\x{4DFF}", "\x{4E00}")); # U+4DFF is not CJK.
ok($ignoreCJK->lt("Pe\x{5B57}rl", "Perl")); # 'r' is unassigned.

##### 106..110

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

##### 111..115

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

##### 116..120

my $dropArticles = Unicode::Collate->new(
  table => "keys.txt",
  normalization => undef,
  preprocess => sub {
    my $string = shift;
    $string =~ s/\b(?:an?|the)\s+//ig;
    $string;
  },
);

ok($dropArticles->eq("camel", "a    camel"));
ok($dropArticles->eq("Perl", "The Perl"));
ok($dropArticles->lt("the pen", "a pencil"));
ok($Collator->lt("Perl", "The Perl"));
ok($Collator->gt("the pen", "a pencil"));

##### 121..122

my $backLevel1 = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  backwards => [ 1 ],
);

# all strings are reversed at level 1.

ok($backLevel1->gt("AB", "BA"));
ok($backLevel1->gt("\x{3042}\x{3044}", "\x{3044}\x{3042}"));

##### 123..130

my $backLevel2 = Unicode::Collate->new(
  table => "keys.txt",
  normalization => undef,
  undefName => qr/HANGUL|HIRAGANA|KATAKANA|BOPOMOFO/,
  backwards => 2,
);

ok($backLevel2->gt("Ca\x{300}ca\x{302}", "ca\x{302}ca\x{300}"));
ok($backLevel2->gt("ca\x{300}ca\x{302}", "Ca\x{302}ca\x{300}"));
ok($Collator  ->lt("Ca\x{300}ca\x{302}", "ca\x{302}ca\x{300}"));
ok($Collator  ->lt("ca\x{300}ca\x{302}", "Ca\x{302}ca\x{300}"));

# HIRAGANA and KATAKANA are made undefined via undefName.
# So they are after CJK Unified Ideographs.

ok($backLevel2->lt("\x{4E00}", $hiragana));
ok($backLevel2->lt("\x{4E03}", $katakana));
ok($Collator  ->gt("\x{4E00}", $hiragana));
ok($Collator  ->gt("\x{4E03}", $katakana));

##### 131..142

# According to Conformance Test,
# a L3-ignorable is treated as a completely ignorable.

my $L3ignorable = Unicode::Collate->new(
  alternate => 'Non-ignorable',
  level => 3,
  table => undef,
  normalization => undef,
  entry => <<'ENTRIES',
0000  ; [.0000.0000.0000.0000] # [0000] NULL (in 6429)
0001  ; [.0000.0000.0000.0000] # [0001] START OF HEADING (in 6429)
0591  ; [.0000.0000.0000.0591] # HEBREW ACCENT ETNAHTA
1D165 ; [.0000.0000.0000.1D165] # MUSICAL SYMBOL COMBINING STEM
0021  ; [*024B.0020.0002.0021] # EXCLAMATION MARK
09BE  ; [.114E.0020.0002.09BE] # BENGALI VOWEL SIGN AA
09C7  ; [.1157.0020.0002.09C7] # BENGALI VOWEL SIGN E
09CB  ; [.1159.0020.0002.09CB] # BENGALI VOWEL SIGN O
09C7 09BE ; [.1159.0020.0002.09CB] # BENGALI VOWEL SIGN O
1D1B9 ; [*098A.0020.0002.1D1B9] # MUSICAL SYMBOL SEMIBREVIS WHITE
1D1BA ; [*098B.0020.0002.1D1BA] # MUSICAL SYMBOL SEMIBREVIS BLACK
1D1BB ; [*098A.0020.0002.1D1B9][.0000.0000.0000.1D165] # M.S. MINIMA
1D1BC ; [*098B.0020.0002.1D1BA][.0000.0000.0000.1D165] # M.S. MINIMA BLACK
ENTRIES
);

ok($L3ignorable->lt("\cA", "!"));
ok($L3ignorable->lt("\x{591}", "!"));
ok($L3ignorable->eq("\cA", "\x{591}"));
ok($L3ignorable->eq("\x{09C7}\x{09BE}A", "\x{09C7}\cA\x{09BE}A"));
ok($L3ignorable->eq("\x{09C7}\x{09BE}A", "\x{09C7}\x{0591}\x{09BE}A"));
ok($L3ignorable->eq("\x{09C7}\x{09BE}A", "\x{09C7}\x{1D165}\x{09BE}A"));
ok($L3ignorable->eq("\x{09C7}\x{09BE}A", "\x{09CB}A"));
ok($L3ignorable->lt("\x{1D1BB}", "\x{1D1BC}"));
ok($L3ignorable->eq("\x{1D1BB}", "\x{1D1B9}"));
ok($L3ignorable->eq("\x{1D1BC}", "\x{1D1BA}"));
ok($L3ignorable->eq("\x{1D1BB}", "\x{1D1B9}\x{1D165}"));
ok($L3ignorable->eq("\x{1D1BC}", "\x{1D1BA}\x{1D165}"));

##### 143..149

my $O_str = Unicode::Collate->new(
  table => "keys.txt",
  normalization => undef,
  entry => <<'ENTRIES',
0008  ; [*0008.0000.0000.0000] # BACKSPACE (need to be non-ignorable)
004F 0337 ; [.0B53.0020.0008.004F] # capital O WITH SHORT SOLIDUS OVERLAY
006F 0008 002F ; [.0B53.0020.0002.006F] # LATIN SMALL LETTER O WITH STROKE
004F 0008 002F ; [.0B53.0020.0008.004F] # LATIN CAPITAL LETTER O WITH STROKE
006F 0337 ; [.0B53.0020.0002.004F] # small O WITH SHORT SOLIDUS OVERLAY
200B  ; [.2000.0000.0000.0000] # ZERO WIDTH SPACE (may be non-sense but ...)
#00F8 ; [.0B53.0020.0002.00F8] # LATIN SMALL LETTER O WITH STROKE
#00D8 ; [.0B53.0020.0008.00D8] # LATIN CAPITAL LETTER O WITH STROKE
ENTRIES
);

my $o_BS_slash = _pack_U(0x006F, 0x0008, 0x002F);
my $O_BS_slash = _pack_U(0x004F, 0x0008, 0x002F);
my $o_sol    = _pack_U(0x006F, 0x0337);
my $O_sol    = _pack_U(0x004F, 0x0337);
my $o_stroke = _pack_U(0x00F8);
my $O_stroke = _pack_U(0x00D8);

ok($O_str->eq($o_stroke, $o_BS_slash));
ok($O_str->eq($O_stroke, $O_BS_slash));

ok($O_str->eq($o_stroke, $o_sol));
ok($O_str->eq($O_stroke, $O_sol));

ok($Collator->eq("\x{200B}", "\0"));
ok($O_str   ->gt("\x{200B}", "\0"));
ok($O_str   ->gt("\x{200B}", "A"));

##### 150..159

my %origVer = $Collator->change(UCA_Version => 8);

$Collator->change(level => 3);

ok($Collator->gt("!\x{300}", ""));
ok($Collator->gt("!\x{300}", "!"));
ok($Collator->eq("!\x{300}", "\x{300}"));

$Collator->change(level => 2);

ok($Collator->eq("!\x{300}", "\x{300}"));

$Collator->change(level => 4);

ok($Collator->gt("!\x{300}", "!"));
ok($Collator->lt("!\x{300}", "\x{300}"));

$Collator->change(%origVer, level => 3);

ok($Collator->eq("!\x{300}", ""));
ok($Collator->eq("!\x{300}", "!"));
ok($Collator->lt("!\x{300}", "\x{300}"));

$Collator->change(level => 4);

ok($Collator->gt("!\x{300}", ""));
ok($Collator->eq("!\x{300}", "!"));

#####

