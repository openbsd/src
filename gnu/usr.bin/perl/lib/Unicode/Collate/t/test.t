# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

BEGIN {
    if (ord("A") == 193) {
	print "1..0 # Unicode::Collate not ported to EBCDIC\n";
	exit 0;
    }
}

use Test;
BEGIN { plan tests => 160 };
use Unicode::Collate;
ok(1); # If we made it this far, we're ok.

#########################

my $UCA_Version = "8.0";

ok(Unicode::Collate::UCA_Version, $UCA_Version);
ok(Unicode::Collate->UCA_Version, $UCA_Version);

my $Collator = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
);

ok(ref $Collator, "Unicode::Collate");

ok($Collator->UCA_Version,   $UCA_Version);
ok($Collator->UCA_Version(), $UCA_Version);

ok(
  join(':', $Collator->sort( 
    qw/ lib strict Carp ExtUtils CGI Time warnings Math overload Pod CPAN /
  ) ),
  join(':',
    qw/ Carp CGI CPAN ExtUtils lib Math overload Pod strict Time warnings /
  ),
);

my $A_acute = pack('U', 0x00C1);
my $acute   = pack('U', 0x0301);

ok($Collator->cmp("A$acute", $A_acute), -1);
ok($Collator->cmp("", ""), 0);
ok(! $Collator->ne("", "") );
ok(  $Collator->eq("", "") );
ok($Collator->cmp("", "perl"), -1);

##############

eval { require Unicode::Normalize };

if (!$@) {
  my $NFD = Unicode::Collate->new(
    table => 'keys.txt',
    entry => <<'ENTRIES',
0430 ; [.0B01.0020.0002.0430] # CYRILLIC SMALL LETTER A
0410 ; [.0B01.0020.0008.0410] # CYRILLIC CAPITAL LETTER A
04D3 ; [.0B09.0020.0002.04D3] # CYRILLIC SMALL LETTER A WITH DIAERESIS
0430 0308 ; [.0B09.0020.0002.04D3] # CYRILLIC SMALL LETTER A WITH DIAERESIS
04D3 ; [.0B09.0020.0002.04D3] # CYRILLIC SMALL LETTER A WITH DIAERESIS
0430 0308 ; [.0B09.0020.0002.04D3] # CYRILLIC SMALL LETTER A WITH DIAERESIS
04D2 ; [.0B09.0020.0008.04D2] # CYRILLIC CAPITAL LETTER A WITH DIAERESIS
0410 0308 ; [.0B09.0020.0008.04D2] # CYRILLIC CAPITAL LETTER A WITH DIAERESIS
0430 3099 ; [.0B10.0020.0002.04D3] # A WITH KATAKANA VOICED
0430 3099 0308 ; [.0B11.0020.0002.04D3] # A WITH KATAKANA VOICED, DIAERESIS
ENTRIES
  );
  ok($NFD->eq("A$acute", $A_acute));
  ok($NFD->eq("\x{4D3}\x{325}", "\x{430}\x{308}\x{325}"));
  ok($NFD->lt("\x{430}\x{308}A", "\x{430}\x{308}B"));
  ok($NFD->lt("\x{430}\x{3099}B", "\x{430}\x{308}\x{3099}A"));
  ok($NFD->eq("\x{0430}\x{3099}\x{309A}\x{0308}",
              "\x{0430}\x{309A}\x{3099}\x{0308}") );
}
else {
  ok(1);
  ok(1);
  ok(1);
  ok(1);
  ok(1);
}

##############

my $trad = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
  ignoreName => qr/HANGUL|HIRAGANA|KATAKANA|BOPOMOFO/,
  level => 4,
  entry => << 'ENTRIES',
 0063 0068 ; [.0893.0020.0002.0063] % "ch" in traditional Spanish
 0043 0068 ; [.0893.0020.0008.0043] # "Ch" in traditional Spanish
 00DF ; [.09F3.0154.0004.00DF] [.09F3.0020.0004.00DF] # eszet in Germany
ENTRIES
);

ok(
  join(':', $trad->sort( qw/ acha aca ada acia acka / ) ),
  join(':',              qw/ aca acia acka acha ada / ),
);

ok(
  join(':', $Collator->sort( qw/ acha aca ada acia acka / ) ),
  join(':',                  qw/ aca acha acia acka ada / ),
);

my $hiragana = "\x{3042}\x{3044}";
my $katakana = "\x{30A2}\x{30A4}";

# HIRAGANA and KATAKANA are ignorable via ignoreName
ok($trad->eq($hiragana, ""));
ok($trad->eq("", $katakana));
ok($trad->eq($hiragana, $katakana));
ok($trad->eq($katakana, $hiragana));

##############

my $old_level = $Collator->{level};

$Collator->{level} = 2;

ok( $Collator->cmp("ABC","abc"), 0);
ok( $Collator->eq("ABC","abc") );
ok( $Collator->le("ABC","abc") );
ok( $Collator->cmp($hiragana, $katakana), 0);
ok( $Collator->eq($hiragana, $katakana) );
ok( $Collator->ge($hiragana, $katakana) );

# hangul
ok( $Collator->eq("a\x{AC00}b", "a\x{1100}\x{1161}b") );
ok( $Collator->eq("a\x{AE00}b", "a\x{1100}\x{1173}\x{11AF}b") );
ok( $Collator->gt("a\x{AE00}b", "a\x{1100}\x{1173}b\x{11AF}") );
ok( $Collator->lt("a\x{AC00}b", "a\x{AE00}b") );
ok( $Collator->gt("a\x{D7A3}b", "a\x{C544}b") );
ok( $Collator->lt("a\x{C544}b", "a\x{30A2}b") ); # hangul < hiragana

$Collator->{level} = $old_level;

$Collator->{katakana_before_hiragana} = 1;

ok( $Collator->cmp("abc", "ABC"), -1);
ok( $Collator->ne("abc", "ABC") );
ok( $Collator->lt("abc", "ABC") );
ok( $Collator->le("abc", "ABC") );
ok( $Collator->cmp($hiragana, $katakana), 1);
ok( $Collator->ne($hiragana, $katakana) );
ok( $Collator->gt($hiragana, $katakana) );
ok( $Collator->ge($hiragana, $katakana) );

$Collator->{upper_before_lower} = 1;

ok( $Collator->cmp("abc", "ABC"), 1);
ok( $Collator->ge("abc", "ABC"), 1);
ok( $Collator->gt("abc", "ABC"), 1);
ok( $Collator->cmp($hiragana, $katakana), 1);
ok( $Collator->ge($hiragana, $katakana), 1);
ok( $Collator->gt($hiragana, $katakana), 1);

$Collator->{katakana_before_hiragana} = 0;

ok( $Collator->cmp("abc", "ABC"), 1);
ok( $Collator->cmp($hiragana, $katakana), -1);

$Collator->{upper_before_lower} = 0;

ok( $Collator->cmp("abc", "ABC"), -1);
ok( $Collator->le("abc", "ABC") );
ok( $Collator->cmp($hiragana, $katakana), -1);
ok( $Collator->lt($hiragana, $katakana) );

##############

my $ignoreAE = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
  ignoreChar => qr/^[aAeE]$/,
);

ok($ignoreAE->eq("element","lament"));
ok($ignoreAE->eq("Perl","ePrl"));

##############

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

##############

my $undefAE = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
  undefChar => qr/^[aAeE]$/,
);

ok($undefAE ->gt("edge","fog"));
ok($Collator->lt("edge","fog"));
ok($undefAE ->gt("lake","like"));
ok($Collator->lt("lake","like"));

##############

$Collator->{level} = 2;

my $str;

my $orig = "This is a Perl book.";
my $sub = "PERL";
my $rep = "camel";
my $ret = "This is a camel book.";

$str = $orig;
if (my($pos,$len) = $Collator->index($str, $sub)) {
  substr($str, $pos, $len, $rep);
}

ok($str, $ret);

$Collator->{level} = $old_level;

$str = $orig;
if (my($pos,$len) = $Collator->index($str, $sub)) {
  substr($str, $pos, $len, $rep);
}

ok($str, $orig);

##############

my $match;

$Collator->{level} = 1;

$str = "Pe\x{300}rl";
$sub = "pe";
$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, "Pe\x{300}");

$str = "P\x{300}e\x{300}\x{301}\x{303}rl";
$sub = "pE";
$match = undef;
if (my($pos, $len) = $Collator->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, "P\x{300}e\x{300}\x{301}\x{303}");

$Collator->{level} = $old_level;

##############

$trad->{level} = 1;

$str = "Ich mu\x{00DF} studieren.";
$sub = "m\x{00FC}ss";
$match = undef;
if (my($pos, $len) = $trad->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, "mu\x{00DF}");

$trad->{level} = $old_level;

$str = "Ich mu\x{00DF} studieren.";
$sub = "m\x{00FC}ss";
$match = undef;

if (my($pos, $len) = $trad->index($str, $sub)) {
    $match = substr($str, $pos, $len);
}
ok($match, undef);

$match = undef;
if (my($pos,$len) = $Collator->index("", "")) {
    $match = substr("", $pos, $len);
}
ok($match, "");

$match = undef;
if (my($pos,$len) = $Collator->index("", "abc")) {
    $match = substr("", $pos, $len);
}
ok($match, undef);

##############

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


##############

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

##############

my $all_undef = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  overrideCJK => undef,
  overrideHangul => undef,
);

# All in the Unicode code point order.
# No hangul decomposition.

ok($all_undef->lt("\x{3042}", "\x{4E00}"));
ok($all_undef->lt("\x{4DFF}", "\x{4E00}"));
ok($all_undef->lt("\x{4E00}", "\x{AC00}"));
ok($all_undef->gt("\x{AC00}", "\x{1100}\x{1161}"));
ok($all_undef->gt("\x{AC00}", "\x{ABFF}"));

##############

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

##############

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

##############

my $blanked = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
  alternate => 'Blanked',
);

ok($blanked->lt("death", "de luge"));
ok($blanked->lt("de luge", "de-luge"));
ok($blanked->lt("de-luge", "deluge"));
ok($blanked->lt("deluge", "de\x{2010}luge"));
ok($blanked->lt("deluge", "de Luge"));

##############

my $nonIgn = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
  alternate => 'Non-ignorable',
);

ok($nonIgn->lt("de luge", "de Luge"));
ok($nonIgn->lt("de Luge", "de-luge"));
ok($nonIgn->lt("de-Luge", "de\x{2010}luge"));
ok($nonIgn->lt("de-luge", "death"));
ok($nonIgn->lt("death", "deluge"));

##############

my $shifted = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
  alternate => 'Shifted',
);

ok($shifted->lt("death", "de luge"));
ok($shifted->lt("de luge", "de-luge"));
ok($shifted->lt("de-luge", "deluge"));
ok($shifted->lt("deluge", "de Luge"));
ok($shifted->lt("de Luge", "deLuge"));

##############

my $shTrim = Unicode::Collate->new(
  table => 'keys.txt',
  normalization => undef,
  alternate => 'Shift-Trimmed',
);

ok($shTrim->lt("death", "deluge"));
ok($shTrim->lt("deluge", "de luge"));
ok($shTrim->lt("de luge", "de-luge"));
ok($shTrim->lt("de-luge", "deLuge"));
ok($shTrim->lt("deLuge", "de Luge"));

##############

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

##############

# rearranged : 0x0E40..0x0E44, 0x0EC0..0x0EC4

ok($Collator->lt("A", "B"));
ok($Collator->lt("\x{0E40}", "\x{0E41}"));
ok($Collator->lt("\x{0E40}A", "\x{0E41}B"));
ok($Collator->lt("\x{0E41}A", "\x{0E40}B"));
ok($Collator->lt("A\x{0E41}A", "A\x{0E40}B"));

ok($all_undef->lt("A", "B"));
ok($all_undef->lt("\x{0E40}", "\x{0E41}"));
ok($all_undef->lt("\x{0E40}A", "\x{0E41}B"));
ok($all_undef->lt("\x{0E41}A", "\x{0E40}B"));
ok($all_undef->lt("A\x{0E41}A", "A\x{0E40}B"));

##############

my $no_rearrange = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  rearrange => [],
);

ok($no_rearrange->lt("A", "B"));
ok($no_rearrange->lt("\x{0E40}", "\x{0E41}"));
ok($no_rearrange->lt("\x{0E40}A", "\x{0E41}B"));
ok($no_rearrange->gt("\x{0E41}A", "\x{0E40}B"));
ok($no_rearrange->gt("A\x{0E41}A", "A\x{0E40}B"));

##############

# equivalent to $no_rearrange

my $undef_rearrange = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  rearrange => undef,
);

ok($undef_rearrange->lt("A", "B"));
ok($undef_rearrange->lt("\x{0E40}", "\x{0E41}"));
ok($undef_rearrange->lt("\x{0E40}A", "\x{0E41}B"));
ok($undef_rearrange->gt("\x{0E41}A", "\x{0E40}B"));
ok($undef_rearrange->gt("A\x{0E41}A", "A\x{0E40}B"));

##############

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

##############

my $backLevel1 = Unicode::Collate->new(
  table => undef,
  normalization => undef,
  backwards => [ 1 ],
);

# all strings are reversed at level 1.

ok($backLevel1->gt("AB", "BA"));
ok($backLevel1->gt("\x{3042}\x{3044}", "\x{3044}\x{3042}"));

##############

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

##############
