#!./perl

print "1..51\n";

my $test = 1;

sub ok {
    if ($_[0]) {
	if ($_[1]) {
	    print "ok $test - $_[1]\n";
	} else {
	    print "ok $test\n";
	}
    } else {
	if ($_[1]) {
	    print "not ok $test - $_[1]\n";
	} else {
	    print "not ok $test\n";
	}
    }
    $test++;
}

$a = "HELLO.* world";
$b = "hello.* WORLD";

ok("\Q$a\E."      eq "HELLO\\.\\*\\ world.", '\Q\E HELLO.* world');
ok("\u$a"         eq "HELLO\.\* world",      '\u');
ok("\l$a"         eq "hELLO\.\* world",      '\l');
ok("\U$a"         eq "HELLO\.\* WORLD",      '\U');
ok("\L$a"         eq "hello\.\* world",      '\L');

ok(quotemeta($a)  eq "HELLO\\.\\*\\ world",  'quotemeta');
ok(ucfirst($a)    eq "HELLO\.\* world",      'ucfirst');
ok(lcfirst($a)    eq "hELLO\.\* world",      'lcfirst');
ok(uc($a)         eq "HELLO\.\* WORLD",      'uc');
ok(lc($a)         eq "hello\.\* world",      'lc');

ok("\Q$b\E."      eq "hello\\.\\*\\ WORLD.", '\Q\E hello.* WORLD');
ok("\u$b"         eq "Hello\.\* WORLD",      '\u');
ok("\l$b"         eq "hello\.\* WORLD",      '\l');
ok("\U$b"         eq "HELLO\.\* WORLD",      '\U');
ok("\L$b"         eq "hello\.\* world",      '\L');

ok(quotemeta($b)  eq "hello\\.\\*\\ WORLD",  'quotemeta');
ok(ucfirst($b)    eq "Hello\.\* WORLD",      'ucfirst');
ok(lcfirst($b)    eq "hello\.\* WORLD",      'lcfirst');
ok(uc($b)         eq "HELLO\.\* WORLD",      'uc');
ok(lc($b)         eq "hello\.\* world",      'lc');

# \x{100} is LATIN CAPITAL LETTER A WITH MACRON; its bijective lowercase is
# \x{101}, LATIN SMALL LETTER A WITH MACRON.

$a = "\x{100}\x{101}Aa";
$b = "\x{101}\x{100}aA";

ok("\Q$a\E."      eq "\x{100}\x{101}Aa.", '\Q\E \x{100}\x{101}Aa');
ok("\u$a"         eq "\x{100}\x{101}Aa",  '\u');
ok("\l$a"         eq "\x{101}\x{101}Aa",  '\l');
ok("\U$a"         eq "\x{100}\x{100}AA",  '\U');
ok("\L$a"         eq "\x{101}\x{101}aa",  '\L');

ok(quotemeta($a)  eq "\x{100}\x{101}Aa",  'quotemeta');
ok(ucfirst($a)    eq "\x{100}\x{101}Aa",  'ucfirst');
ok(lcfirst($a)    eq "\x{101}\x{101}Aa",  'lcfirst');
ok(uc($a)         eq "\x{100}\x{100}AA",  'uc');
ok(lc($a)         eq "\x{101}\x{101}aa",  'lc');

ok("\Q$b\E."      eq "\x{101}\x{100}aA.", '\Q\E \x{101}\x{100}aA');
ok("\u$b"         eq "\x{100}\x{100}aA",  '\u');
ok("\l$b"         eq "\x{101}\x{100}aA",  '\l');
ok("\U$b"         eq "\x{100}\x{100}AA",  '\U');
ok("\L$b"         eq "\x{101}\x{101}aa",  '\L');

ok(quotemeta($b)  eq "\x{101}\x{100}aA",  'quotemeta');
ok(ucfirst($b)    eq "\x{100}\x{100}aA",  'ucfirst');
ok(lcfirst($b)    eq "\x{101}\x{100}aA",  'lcfirst');
ok(uc($b)         eq "\x{100}\x{100}AA",  'uc');
ok(lc($b)         eq "\x{101}\x{101}aa",  'lc');

# \x{DF} is LATIN SMALL LETTER SHARP S, its uppercase is SS or \x{53}\x{53};
# \x{149} is LATIN SMALL LETTER N PRECEDED BY APOSTROPHE, its uppercase is
# \x{2BC}\x{E4} or MODIFIER LETTER APOSTROPHE and N.

# In EBCDIC \x{DF} is LATIN SMALL LETTER Y WITH DIAERESIS,
# and it's uppercase is \x{178}, LATIN CAPITAL LETTER Y WITH DIAERESIS.

if (ord("A") == 193) { # EBCDIC
    ok("\U\x{DF}aB\x{149}cD" eq "\x{178}AB\x{2BC}NCD",
       "multicharacter uppercase");
} elsif (ord("A") == 65) {
    ok("\U\x{DF}aB\x{149}cD" eq "SSAB\x{2BC}NCD",
       "multicharacter uppercase");
} else {
    ok(0, "what is your encoding?");
}

# The \x{DF} is its own lowercase, ditto for \x{149}.
# There are no single character -> multiple characters lowercase mappings.

if (ord("A") == 193) { # EBCDIC
    ok("\LaB\x{149}cD" eq "ab\x{149}cd",
       "multicharacter lowercase");
} elsif (ord("A") == 65) {
    ok("\L\x{DF}aB\x{149}cD" eq "\x{DF}ab\x{149}cd",
       "multicharacter lowercase");
} else {
    ok(0, "what is your encoding?");
}

# titlecase is used for \u / ucfirst.

# \x{587} is ARMENIAN SMALL LIGATURE ECH YIWN and its titlecase is
# \x{535}\x{582} ARMENIAN CAPITAL LETTER ECH + ARMENIAN SMALL LETTER YIWN
# while its lowercase is 
# \x{587} itself
# and its uppercase is
# \x{535}\x{552} ARMENIAN CAPITAL LETTER ECH + ARMENIAN CAPITAL LETTER YIWN

$a = "\x{587}";

ok("\L\x{587}" eq "\x{587}",        "ligature lowercase");
ok("\u\x{587}" eq "\x{535}\x{582}", "ligature titlecase");
ok("\U\x{587}" eq "\x{535}\x{552}", "ligature uppercase");

# mktables had problems where many-to-one case mappings didn't work right.
# The lib/unifold.t should give the fourth folding, "casefolding", a good
# workout.

ok(lc("\x{1C4}") eq "\x{1C6}",      "U+01C4 lc is U+01C6");
ok(lc("\x{1C5}") eq "\x{1C6}",      "U+01C5 lc is U+01C6, too");

ok(ucfirst("\x{3C2}") eq "\x{3A3}", "U+03C2 ucfirst is U+03A3");
ok(ucfirst("\x{3C3}") eq "\x{3A3}", "U+03C3 ucfirst is U+03A3, too");

ok(uc("\x{1C5}") eq "\x{1C4}",      "U+01C5 uc is U+01C4");
ok(uc("\x{1C6}") eq "\x{1C4}",      "U+01C6 uc is U+01C4, too");

