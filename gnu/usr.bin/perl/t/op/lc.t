#!./perl

# This file is intentionally encoded in latin-1.

BEGIN {
    chdir 't';
    @INC = '../lib';
    require Config; import Config;
    require './test.pl';
    require './loc_tools.pl';   # Contains find_utf8_ctype_locale()
}

use feature qw( fc );

plan tests => 134 + 4 * 256;

is(lc(undef),	   "", "lc(undef) is ''");
is(lcfirst(undef), "", "lcfirst(undef) is ''");
is(uc(undef),	   "", "uc(undef) is ''");
is(ucfirst(undef), "", "ucfirst(undef) is ''");

{
    no feature 'fc';
    is(CORE::fc(undef), "", "fc(undef) is ''");
    is(CORE::fc(''),    "", "fc('') is ''");

    local $@;
    eval { fc("eeyup") };
    like($@, qr/Undefined subroutine &main::fc/, "fc() throws an exception,");

    {
        use feature 'fc';
        local $@;
        eval { fc("eeyup") };
        ok(!$@, "...but works after requesting the feature");
    }
}

$a = "HELLO.* world";
$b = "hello.* WORLD";

is("\Q$a\E."      , "HELLO\\.\\*\\ world.", '\Q\E HELLO.* world');
is("\u$a"         , "HELLO\.\* world",      '\u');
is("\l$a"         , "hELLO\.\* world",      '\l');
is("\U$a"         , "HELLO\.\* WORLD",      '\U');
is("\L$a"         , "hello\.\* world",      '\L');
is("\F$a"         , "hello\.\* world",      '\F');

is(quotemeta($a)  , "HELLO\\.\\*\\ world",  'quotemeta');
is(ucfirst($a)    , "HELLO\.\* world",      'ucfirst');
is(lcfirst($a)    , "hELLO\.\* world",      'lcfirst');
is(uc($a)         , "HELLO\.\* WORLD",      'uc');
is(lc($a)         , "hello\.\* world",      'lc');
is(fc($a)         , "hello\.\* world",      'fc');

is("\Q$b\E."      , "hello\\.\\*\\ WORLD.", '\Q\E hello.* WORLD');
is("\u$b"         , "Hello\.\* WORLD",      '\u');
is("\l$b"         , "hello\.\* WORLD",      '\l');
is("\U$b"         , "HELLO\.\* WORLD",      '\U');
is("\L$b"         , "hello\.\* world",      '\L');
is("\F$b"         , "hello\.\* world",      '\F');

is(quotemeta($b)  , "hello\\.\\*\\ WORLD",  'quotemeta');
is(ucfirst($b)    , "Hello\.\* WORLD",      'ucfirst');
is(lcfirst($b)    , "hello\.\* WORLD",      'lcfirst');
is(uc($b)         , "HELLO\.\* WORLD",      'uc');
is(lc($b)         , "hello\.\* world",      'lc');
is(fc($b)         , "hello\.\* world",      'fc');

# \x{100} is LATIN CAPITAL LETTER A WITH MACRON; its bijective lowercase is
# \x{101}, LATIN SMALL LETTER A WITH MACRON.
# Which is also its foldcase.

$a = "\x{100}\x{101}Aa";
$b = "\x{101}\x{100}aA";

is("\Q$a\E."      , "\x{100}\x{101}Aa.", '\Q\E \x{100}\x{101}Aa');
is("\u$a"         , "\x{100}\x{101}Aa",  '\u');
is("\l$a"         , "\x{101}\x{101}Aa",  '\l');
is("\U$a"         , "\x{100}\x{100}AA",  '\U');
is("\L$a"         , "\x{101}\x{101}aa",  '\L');
is("\F$a"         , "\x{101}\x{101}aa",  '\F');

is(quotemeta($a)  , "\x{100}\x{101}Aa",  'quotemeta');
is(ucfirst($a)    , "\x{100}\x{101}Aa",  'ucfirst');
is(lcfirst($a)    , "\x{101}\x{101}Aa",  'lcfirst');
is(uc($a)         , "\x{100}\x{100}AA",  'uc');
is(lc($a)         , "\x{101}\x{101}aa",  'lc');
is(fc($a)         , "\x{101}\x{101}aa",  'fc');

is("\Q$b\E."      , "\x{101}\x{100}aA.", '\Q\E \x{101}\x{100}aA');
is("\u$b"         , "\x{100}\x{100}aA",  '\u');
is("\l$b"         , "\x{101}\x{100}aA",  '\l');
is("\U$b"         , "\x{100}\x{100}AA",  '\U');
is("\L$b"         , "\x{101}\x{101}aa",  '\L');
is("\F$b"         , "\x{101}\x{101}aa",  '\F');

is(quotemeta($b)  , "\x{101}\x{100}aA",  'quotemeta');
is(ucfirst($b)    , "\x{100}\x{100}aA",  'ucfirst');
is(lcfirst($b)    , "\x{101}\x{100}aA",  'lcfirst');
is(uc($b)         , "\x{100}\x{100}AA",  'uc');
is(lc($b)         , "\x{101}\x{101}aa",  'lc');
is(fc($b)         , "\x{101}\x{101}aa",  'fc');

# \x{DF} is LATIN SMALL LETTER SHARP S, its uppercase is SS or \x{53}\x{53};
# \x{149} is LATIN SMALL LETTER N PRECEDED BY APOSTROPHE, its uppercase is
# \x{2BC}\x{E4} or MODIFIER LETTER APOSTROPHE and N.

is(latin1_to_native("\U\x{DF}aB\x{149}cD"), latin1_to_native("SSAB\x{2BC}NCD"),
       "multicharacter uppercase");

# The \x{DF} is its own lowercase, ditto for \x{149}.
# There are no single character -> multiple characters lowercase mappings.

is(latin1_to_native("\L\x{DF}aB\x{149}cD"), latin1_to_native("\x{DF}ab\x{149}cd"),
       "multicharacter lowercase");

# \x{DF} is LATIN SMALL LETTER SHARP S, its foldcase is ss or \x{73}\x{73};
# \x{149} is LATIN SMALL LETTER N PRECEDED BY APOSTROPHE, its foldcase is
# \x{2BC}\x{6E} or MODIFIER LETTER APOSTROPHE and n.
# Note that is this further tested in t/uni/fold.t

is(latin1_to_native("\F\x{DF}aB\x{149}cD"), latin1_to_native("ssab\x{2BC}ncd"),
       "multicharacter foldcase");


# titlecase is used for \u / ucfirst.

# \x{587} is ARMENIAN SMALL LIGATURE ECH YIWN and its titlecase is
# \x{535}\x{582} ARMENIAN CAPITAL LETTER ECH + ARMENIAN SMALL LETTER YIWN
# while its lowercase is 
# \x{587} itself
# and its uppercase is
# \x{535}\x{552} ARMENIAN CAPITAL LETTER ECH + ARMENIAN CAPITAL LETTER YIWN
# The foldcase is \x{565}\x{582} ARMENIAN SMALL LETTER ECH + ARMENIAN SMALL LETTER YIWN

$a = "\x{587}";

is("\L\x{587}" , "\x{587}",        "ligature lowercase");
is("\u\x{587}" , "\x{535}\x{582}", "ligature titlecase");
is("\U\x{587}" , "\x{535}\x{552}", "ligature uppercase");
is("\F\x{587}" , "\x{565}\x{582}", "ligature foldcase");

# mktables had problems where many-to-one case mappings didn't work right.
# The lib/uni/fold.t should give the fourth folding, "casefolding", a good
# workout.
# \x{01C4} is LATIN CAPITAL LETTER DZ WITH CARON
# \x{01C5} is LATIN CAPITAL LETTER D WITH SMALL LETTER Z WITH CARON
# \x{01C6} is LATIN SMALL LETTER DZ WITH CARON
# \x{03A3} is GREEK CAPITAL LETTER SIGMA
# \x{03C2} is GREEK SMALL LETTER FINAL SIGMA
# \x{03C3} is GREEK SMALL LETTER SIGMA

is(lc("\x{1C4}") , "\x{1C6}",      "U+01C4 lc is U+01C6");
is(lc("\x{1C5}") , "\x{1C6}",      "U+01C5 lc is U+01C6, too");

is(ucfirst("\x{3C2}") , "\x{3A3}", "U+03C2 ucfirst is U+03A3");
is(ucfirst("\x{3C3}") , "\x{3A3}", "U+03C3 ucfirst is U+03A3, too");

is(uc("\x{1C5}") , "\x{1C4}",      "U+01C5 uc is U+01C4");
is(uc("\x{1C6}") , "\x{1C4}",      "U+01C6 uc is U+01C4, too");

# #18107: A host of bugs involving [ul]c{,first}. AMS 20021106
$a = "\x{3c3}foo.bar"; # \x{3c3} == GREEK SMALL LETTER SIGMA.
$b = "\x{3a3}FOO.BAR"; # \x{3a3} == GREEK CAPITAL LETTER SIGMA.

($c = $b) =~ s/(\w+)/lc($1)/ge;
is($c , $a, "Using s///e to change case.");

($c = $a) =~ s/(\p{IsWord}+)/uc($1)/ge;
is($c , $b, "Using s///e to change case.");

($c = $a) =~ s/(\p{IsWord}+)/fc($1)/ge;
is($c , $a, "Using s///e to foldcase.");

($c = $b) =~ s/(\p{IsWord}+)/lcfirst($1)/ge;
is($c , "\x{3c3}FOO.bAR", "Using s///e to change case.");

($c = $a) =~ s/(\p{IsWord}+)/ucfirst($1)/ge;
is($c , "\x{3a3}foo.Bar", "Using s///e to change case.");

# #18931: perl5.8.0 bug in \U..\E processing
# Test case from Nicholas Clark.
for my $a (0,1) {
    $_ = 'abcdefgh';
    $_ .= chr 256;
    chop;
    /(.*)/;
    is(uc($1), "ABCDEFGH", "[perl #18931]");
}

{
    foreach (0, 1) {
	$a = v10.v257;
	chop $a;
	$a =~ s/^(\s*)(\w*)/$1\u$2/;
	is($a, v10, "[perl #18857]");
    } 
}


# [perl #38619] Bug in lc and uc (interaction between UTF-8, substr, and lc/uc)

for ("a\x{100}", "xyz\x{100}") {
    is(substr(uc($_), 0), uc($_), "[perl #38619] uc");
}
for ("A\x{100}", "XYZ\x{100}") {
    is(substr(lc($_), 0), lc($_), "[perl #38619] lc");
}
for ("a\x{100}", "ßyz\x{100}") { # ß to Ss (different length)
    is(substr(ucfirst($_), 0), ucfirst($_), "[perl #38619] ucfirst");
}

#fc() didn't exist back then, but coverage is coverage.
for ("a\x{100}", "ßyz\x{100}", "xyz\x{100}", "XYZ\x{100}") { # ß to Ss (different length)
    is(substr(fc($_), 0), fc($_), "[perl #38619] fc");
}

# Related to [perl #38619]
# the original report concerns PERL_MAGIC_utf8.
# these cases concern PERL_MAGIC_regex_global.

for (map { $_ } "a\x{100}", "abc\x{100}", "\x{100}") {
    chop; # get ("a", "abc", "") in utf8
    my $return =  uc($_) =~ /\G(.?)/g;
    my $result = $return ? $1 : "not";
    my $expect = (uc($_) =~ /(.?)/g)[0];
    is($return, 1,       "[perl #38619]");
    is($result, $expect, "[perl #38619]");
}

for (map { $_ } "A\x{100}", "ABC\x{100}", "\x{100}") {
    chop; # get ("A", "ABC", "") in utf8
    my $return =  lc($_) =~ /\G(.?)/g;
    my $result = $return ? $1 : "not";
    my $expect = (lc($_) =~ /(.?)/g)[0];
    is($return, 1,       "[perl #38619]");
    is($result, $expect, "[perl #38619]");
}

for (map { $_ } "A\x{100}", "ABC\x{100}", "\x{100}") {
    chop; # get ("A", "ABC", "") in utf8
    my $return =  fc($_) =~ /\G(.?)/g;
    my $result = $return ? $1 : "not";
    my $expect = (fc($_) =~ /(.?)/g)[0];
    is($return, 1,       "[perl #38619]");
    is($result, $expect, "[perl #38619]");
}

for (1, 4, 9, 16, 25) {
    is(uc "\x{03B0}" x $_, "\x{3a5}\x{308}\x{301}" x $_,
       'uc U+03B0 grows threefold');

    is(lc "\x{0130}" x $_, "i\x{307}" x $_, 'lc U+0130 grows');

    is(fc "\x{03B0}" x $_, "\x{3C5}\x{308}\x{301}" x $_,
       'fc U+03B0 grows threefold');
}

# bug #43207
my $temp = "HellO";
for ("$temp") {
    lc $_;
    is($_, "HellO", '[perl #43207] lc($_) modifying $_');
}
for ("$temp") {
    fc $_;
    is($_, "HellO", '[perl #43207] fc($_) modifying $_');
}
for ("$temp") {
    uc $_;
    is($_, "HellO", '[perl #43207] uc($_) modifying $_');
}
for ("$temp") {
    ucfirst $_;
    is($_, "HellO", '[perl #43207] ucfirst($_) modifying $_');
}
for ("$temp") {
    lcfirst $_;
    is($_, "HellO", '[perl #43207] lcfirst($_) modifying $_');
}

# new in Unicode 5.1.0
is(lc("\x{1E9E}"), "\x{df}", "lc(LATIN CAPITAL LETTER SHARP S)");

{
    use feature 'unicode_strings';
    use bytes;
    is(lc("\xc0"), "\xc0", "lc of above-ASCII Latin1 is itself under use bytes");
    is(lcfirst("\xc0"), "\xc0", "lcfirst of above-ASCII Latin1 is itself under use bytes");
    is(uc("\xe0"), "\xe0", "uc of above-ASCII Latin1 is itself under use bytes");
    is(ucfirst("\xe0"), "\xe0", "ucfirst of above-ASCII Latin1 is itself under use bytes");
}

# Brought up in ticket #117855: Constant folding applied to uc() should use
# the right set of hints.
fresh_perl_like(<<'constantfolding', qr/^(\d+),\1\z/, {},
    my $function = "uc";
    my $char = "\xff";
    {
        use feature 'unicode_strings';
        print ord uc($char), ",",
              ord eval "$function('$char')", "\n";
    }
constantfolding
    'folded uc() in string eval uses the right hints');

# In-place lc/uc should not corrupt string buffers when given a non-utf8-
# flagged thingy that stringifies to utf8
$h{k} = bless[], "\x{3b0}\x{3b0}\x{3b0}bcde"; # U+03B0 grows with uc()
   # using delete marks it as TEMP, so uc-in-place is permitted
like uc delete $h{k}, qr "^(?:\x{3a5}\x{308}\x{301}){3}BCDE=ARRAY\(.*\)",
    'uc(TEMP ref) does not produce a corrupt string';
$h{k} = bless[], "\x{130}bcde"; # U+0130 grows with lc()
   # using delete marks it as TEMP, so uc-in-place is permitted
like lc delete $h{k}, qr "^i\x{307}bcde=array\(.*\)",
    'lc(TEMP ref) does not produce a corrupt string';


my $utf8_locale = find_utf8_ctype_locale();

SKIP: {
    skip 'Can\'t find a UTF-8 locale', 4*256 unless defined $utf8_locale;

    use feature qw( unicode_strings );

    no locale;

    my @unicode_lc;
    my @unicode_uc;
    my @unicode_lcfirst;
    my @unicode_ucfirst;

    # Get all the values outside of 'locale'
    for my $i (0 .. 255) {
        push @unicode_lc, lc(chr $i);
        push @unicode_uc, uc(chr $i);
        push @unicode_lcfirst, lcfirst(chr $i);
        push @unicode_ucfirst, ucfirst(chr $i);
    }

    use if $Config{d_setlocale}, qw(POSIX locale_h);
    use locale;
    setlocale(LC_CTYPE, $utf8_locale);

    for my $i (0 .. 255) {
        is(lc(chr $i), $unicode_lc[$i], "In a UTF-8 locale, lc(chr $i) is the same as official Unicode");
        is(uc(chr $i), $unicode_uc[$i], "In a UTF-8 locale, uc(chr $i) is the same as official Unicode");
        is(lcfirst(chr $i), $unicode_lcfirst[$i], "In a UTF-8 locale, lcfirst(chr $i) is the same as official Unicode");
        is(ucfirst(chr $i), $unicode_ucfirst[$i], "In a UTF-8 locale, ucfirst(chr $i) is the same as official Unicode");
    }
}
