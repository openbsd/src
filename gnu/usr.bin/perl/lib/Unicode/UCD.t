#!perl -w
BEGIN {
    if (ord("A") == 193) {
	print "1..0 # Skip: EBCDIC\n";
	exit 0;
    }
    chdir 't' if -d 't';
    @INC = '../lib';
    @INC = "::lib" if $^O eq 'MacOS'; # module parses @INC itself
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bStorable\b/) {
        print "1..0 # Skip: Storable was not built; Unicode::UCD uses Storable\n";
        exit 0;
    }
}

use strict;
use Unicode::UCD;
use Test::More;

BEGIN { plan tests => 179 };

use Unicode::UCD 'charinfo';

my $charinfo;

$charinfo = charinfo(0x41);

is($charinfo->{code},           '0041', 'LATIN CAPITAL LETTER A');
is($charinfo->{name},           'LATIN CAPITAL LETTER A');
is($charinfo->{category},       'Lu');
is($charinfo->{combining},      '0');
is($charinfo->{bidi},           'L');
is($charinfo->{decomposition},  '');
is($charinfo->{decimal},        '');
is($charinfo->{digit},          '');
is($charinfo->{numeric},        '');
is($charinfo->{mirrored},       'N');
is($charinfo->{unicode10},      '');
is($charinfo->{comment},        '');
is($charinfo->{upper},          '');
is($charinfo->{lower},          '0061');
is($charinfo->{title},          '');
is($charinfo->{block},          'Basic Latin');
is($charinfo->{script},         'Latin');

$charinfo = charinfo(0x100);

is($charinfo->{code},           '0100', 'LATIN CAPITAL LETTER A WITH MACRON');
is($charinfo->{name},           'LATIN CAPITAL LETTER A WITH MACRON');
is($charinfo->{category},       'Lu');
is($charinfo->{combining},      '0');
is($charinfo->{bidi},           'L');
is($charinfo->{decomposition},  '0041 0304');
is($charinfo->{decimal},        '');
is($charinfo->{digit},          '');
is($charinfo->{numeric},        '');
is($charinfo->{mirrored},       'N');
is($charinfo->{unicode10},      'LATIN CAPITAL LETTER A MACRON');
is($charinfo->{comment},        '');
is($charinfo->{upper},          '');
is($charinfo->{lower},          '0101');
is($charinfo->{title},          '');
is($charinfo->{block},          'Latin Extended-A');
is($charinfo->{script},         'Latin');

# 0x0590 is in the Hebrew block but unused.

$charinfo = charinfo(0x590);

is($charinfo->{code},          undef,	'0x0590 - unused Hebrew');
is($charinfo->{name},          undef);
is($charinfo->{category},      undef);
is($charinfo->{combining},     undef);
is($charinfo->{bidi},          undef);
is($charinfo->{decomposition}, undef);
is($charinfo->{decimal},       undef);
is($charinfo->{digit},         undef);
is($charinfo->{numeric},       undef);
is($charinfo->{mirrored},      undef);
is($charinfo->{unicode10},     undef);
is($charinfo->{comment},       undef);
is($charinfo->{upper},         undef);
is($charinfo->{lower},         undef);
is($charinfo->{title},         undef);
is($charinfo->{block},         undef);
is($charinfo->{script},        undef);

# 0x05d0 is in the Hebrew block and used.

$charinfo = charinfo(0x5d0);

is($charinfo->{code},           '05D0', '05D0 - used Hebrew');
is($charinfo->{name},           'HEBREW LETTER ALEF');
is($charinfo->{category},       'Lo');
is($charinfo->{combining},      '0');
is($charinfo->{bidi},           'R');
is($charinfo->{decomposition},  '');
is($charinfo->{decimal},        '');
is($charinfo->{digit},          '');
is($charinfo->{numeric},        '');
is($charinfo->{mirrored},       'N');
is($charinfo->{unicode10},      '');
is($charinfo->{comment},        '');
is($charinfo->{upper},          '');
is($charinfo->{lower},          '');
is($charinfo->{title},          '');
is($charinfo->{block},          'Hebrew');
is($charinfo->{script},         'Hebrew');

# An open syllable in Hangul.

$charinfo = charinfo(0xAC00);

is($charinfo->{code},           'AC00', 'HANGUL SYLLABLE-AC00');
is($charinfo->{name},           'HANGUL SYLLABLE-AC00');
is($charinfo->{category},       'Lo');
is($charinfo->{combining},      '0');
is($charinfo->{bidi},           'L');
is($charinfo->{decomposition},  undef);
is($charinfo->{decimal},        '');
is($charinfo->{digit},          '');
is($charinfo->{numeric},        '');
is($charinfo->{mirrored},       'N');
is($charinfo->{unicode10},      '');
is($charinfo->{comment},        '');
is($charinfo->{upper},          '');
is($charinfo->{lower},          '');
is($charinfo->{title},          '');
is($charinfo->{block},          'Hangul Syllables');
is($charinfo->{script},         'Hangul');

# A closed syllable in Hangul.

$charinfo = charinfo(0xAE00);

is($charinfo->{code},           'AE00', 'HANGUL SYLLABLE-AE00');
is($charinfo->{name},           'HANGUL SYLLABLE-AE00');
is($charinfo->{category},       'Lo');
is($charinfo->{combining},      '0');
is($charinfo->{bidi},           'L');
is($charinfo->{decomposition},  undef);
is($charinfo->{decimal},        '');
is($charinfo->{digit},          '');
is($charinfo->{numeric},        '');
is($charinfo->{mirrored},       'N');
is($charinfo->{unicode10},      '');
is($charinfo->{comment},        '');
is($charinfo->{upper},          '');
is($charinfo->{lower},          '');
is($charinfo->{title},          '');
is($charinfo->{block},          'Hangul Syllables');
is($charinfo->{script},         'Hangul');

$charinfo = charinfo(0x1D400);

is($charinfo->{code},           '1D400', 'MATHEMATICAL BOLD CAPITAL A');
is($charinfo->{name},           'MATHEMATICAL BOLD CAPITAL A');
is($charinfo->{category},       'Lu');
is($charinfo->{combining},      '0');
is($charinfo->{bidi},           'L');
is($charinfo->{decomposition},  '<font> 0041');
is($charinfo->{decimal},        '');
is($charinfo->{digit},          '');
is($charinfo->{numeric},        '');
is($charinfo->{mirrored},       'N');
is($charinfo->{unicode10},      '');
is($charinfo->{comment},        '');
is($charinfo->{upper},          '');
is($charinfo->{lower},          '');
is($charinfo->{title},          '');
is($charinfo->{block},          'Mathematical Alphanumeric Symbols');
is($charinfo->{script},         'Common');

use Unicode::UCD qw(charblock charscript);

# 0x0590 is in the Hebrew block but unused.

is(charblock(0x590),          'Hebrew', '0x0590 - Hebrew unused charblock');
is(charscript(0x590),         undef,    '0x0590 - Hebrew unused charscript');

$charinfo = charinfo(0xbe);

is($charinfo->{code},           '00BE', 'VULGAR FRACTION THREE QUARTERS');
is($charinfo->{name},           'VULGAR FRACTION THREE QUARTERS');
is($charinfo->{category},       'No');
is($charinfo->{combining},      '0');
is($charinfo->{bidi},           'ON');
is($charinfo->{decomposition},  '<fraction> 0033 2044 0034');
is($charinfo->{decimal},        '');
is($charinfo->{digit},          '');
is($charinfo->{numeric},        '3/4');
is($charinfo->{mirrored},       'N');
is($charinfo->{unicode10},      'FRACTION THREE QUARTERS');
is($charinfo->{comment},        '');
is($charinfo->{upper},          '');
is($charinfo->{lower},          '');
is($charinfo->{title},          '');
is($charinfo->{block},          'Latin-1 Supplement');
is($charinfo->{script},         'Common');

use Unicode::UCD qw(charblocks charscripts);

my $charblocks = charblocks();

ok(exists $charblocks->{Thai}, 'Thai charblock exists');
is($charblocks->{Thai}->[0]->[0], hex('0e00'));
ok(!exists $charblocks->{PigLatin}, 'PigLatin charblock does not exist');

my $charscripts = charscripts();

ok(exists $charscripts->{Armenian}, 'Armenian charscript exists');
is($charscripts->{Armenian}->[0]->[0], hex('0531'));
ok(!exists $charscripts->{PigLatin}, 'PigLatin charscript does not exist');

my $charscript;

$charscript = charscript("12ab");
is($charscript, 'Ethiopic', 'Ethiopic charscript');

$charscript = charscript("0x12ab");
is($charscript, 'Ethiopic');

$charscript = charscript("U+12ab");
is($charscript, 'Ethiopic');

my $ranges;

$ranges = charscript('Ogham');
is($ranges->[1]->[0], hex('1681'), 'Ogham charscript');
is($ranges->[1]->[1], hex('169a'));

use Unicode::UCD qw(charinrange);

$ranges = charscript('Cherokee');
ok(!charinrange($ranges, "139f"), 'Cherokee charscript');
ok( charinrange($ranges, "13a0"));
ok( charinrange($ranges, "13f4"));
ok(!charinrange($ranges, "13f5"));

is(Unicode::UCD::UnicodeVersion, '4.0.1', 'UnicodeVersion');

use Unicode::UCD qw(compexcl);

ok(!compexcl(0x0100), 'compexcl');
ok( compexcl(0x0958));

use Unicode::UCD qw(casefold);

my $casefold;

$casefold = casefold(0x41);

ok($casefold->{code} eq '0041' &&
   $casefold->{status} eq 'C'  &&
   $casefold->{mapping} eq '0061', 'casefold 0x41');

$casefold = casefold(0xdf);

ok($casefold->{code} eq '00DF' &&
   $casefold->{status} eq 'F'  &&
   $casefold->{mapping} eq '0073 0073', 'casefold 0xDF');

ok(!casefold(0x20));

use Unicode::UCD qw(casespec);

my $casespec;

ok(!casespec(0x41));

$casespec = casespec(0xdf);

ok($casespec->{code} eq '00DF' &&
   $casespec->{lower} eq '00DF'  &&
   $casespec->{title} eq '0053 0073'  &&
   $casespec->{upper} eq '0053 0053' &&
   !defined $casespec->{condition}, 'casespec 0xDF');

$casespec = casespec(0x307);

ok($casespec->{az}->{code} eq '0307' &&
   !defined $casespec->{az}->{lower} &&
   $casespec->{az}->{title} eq '0307'  &&
   $casespec->{az}->{upper} eq '0307' &&
   $casespec->{az}->{condition} eq 'az After_I',
   'casespec 0x307');

# perl #7305 UnicodeCD::compexcl is weird

for (1) {my $a=compexcl $_}
ok(1, 'compexcl read-only $_: perl #7305');
grep {compexcl $_} %{{1=>2}};
ok(1, 'compexcl read-only hash: perl #7305');

is(Unicode::UCD::_getcode('123'),     123, "_getcode(123)");
is(Unicode::UCD::_getcode('0123'),  0x123, "_getcode(0123)");
is(Unicode::UCD::_getcode('0x123'), 0x123, "_getcode(0x123)");
is(Unicode::UCD::_getcode('0X123'), 0x123, "_getcode(0X123)");
is(Unicode::UCD::_getcode('U+123'), 0x123, "_getcode(U+123)");
is(Unicode::UCD::_getcode('u+123'), 0x123, "_getcode(u+123)");
is(Unicode::UCD::_getcode('U+1234'),   0x1234, "_getcode(U+1234)");
is(Unicode::UCD::_getcode('U+12345'), 0x12345, "_getcode(U+12345)");
is(Unicode::UCD::_getcode('123x'),    undef, "_getcode(123x)");
is(Unicode::UCD::_getcode('x123'),    undef, "_getcode(x123)");
is(Unicode::UCD::_getcode('0x123x'),  undef, "_getcode(x123)");
is(Unicode::UCD::_getcode('U+123x'),  undef, "_getcode(x123)");

{
    my $r1 = charscript('Latin');
    my $n1 = @$r1;
    is($n1, 26, "26 ranges in Latin script (Unicode 4.0.0)");
    shift @$r1 while @$r1;
    my $r2 = charscript('Latin');
    is(@$r2, $n1, "modifying results should not mess up internal caches");
}

{
	is(charinfo(0xdeadbeef), undef, "[perl #23273] warnings in Unicode::UCD");
}
