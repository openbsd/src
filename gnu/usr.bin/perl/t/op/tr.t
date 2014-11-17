# tr.t

use utf8;

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 134;

my $Is_EBCDIC = (ord('i') == 0x89 & ord('J') == 0xd1);

$_ = "abcdefghijklmnopqrstuvwxyz";

tr/a-z/A-Z/;

is($_, "ABCDEFGHIJKLMNOPQRSTUVWXYZ",    'uc');

tr/A-Z/a-z/;

is($_, "abcdefghijklmnopqrstuvwxyz",    'lc');

tr/b-y/B-Y/;
is($_, "aBCDEFGHIJKLMNOPQRSTUVWXYz",    'partial uc');


# In EBCDIC 'I' is \xc9 and 'J' is \0xd1, 'i' is \x89 and 'j' is \x91.
# Yes, discontinuities.  Regardless, the \xca in the below should stay
# untouched (and not became \x8a).
{
    no utf8;
    $_ = "I\xcaJ";

    tr/I-J/i-j/;

    is($_, "i\xcaj",    'EBCDIC discontinuity');
}
#


($x = 12) =~ tr/1/3/;
(my $y = 12) =~ tr/1/3/;
($f = 1.5) =~ tr/1/3/;
(my $g = 1.5) =~ tr/1/3/;
is($x + $y + $f + $g, 71,   'tr cancels IOK and NOK');

# /r
$_ = 'adam';
is y/dam/ve/rd, 'eve', '/r';
is $_, 'adam', '/r leaves param alone';
$g = 'ruby';
is $g =~ y/bury/repl/r, 'perl', '/r with explicit param';
is $g, 'ruby', '/r leaves explicit param alone';
is "aaa" =~ y\a\b\r, 'bbb', '/r with constant param';
ok !eval '$_ !~ y///r', "!~ y///r is forbidden";
like $@, qr\^Using !~ with tr///r doesn't make sense\,
  "!~ y///r error message";
{
  my $w;
  my $wc;
  local $SIG{__WARN__} = sub { $w = shift; ++$wc };
  local $^W = 1;
  eval 'y///r; 1';
  like $w, qr '^Useless use of non-destructive transliteration \(tr///r\)',
    '/r warns in void context';
  is $wc, 1, '/r warns just once';
}

# perlbug [ID 20000511.005]
$_ = 'fred';
/([a-z]{2})/;
$1 =~ tr/A-Z//;
s/^(\s*)f/$1F/;
is($_, 'Fred',  'harmless if explicitly not updating');


# A variant of the above, added in 5.7.2
$_ = 'fred';
/([a-z]{2})/;
eval '$1 =~ tr/A-Z/A-Z/;';
s/^(\s*)f/$1F/;
is($_, 'Fred',  'harmless if implicitly not updating');
is($@, '',      '    no error');


# check tr handles UTF8 correctly
($x = 256.65.258) =~ tr/a/b/;
is($x, 256.65.258,  'handles UTF8');
is(length $x, 3);

$x =~ tr/A/B/;
is(length $x, 3);
if (ord("\t") == 9) { # ASCII
    is($x, 256.66.258);
}
else {
    is($x, 256.65.258);
}

# EBCDIC variants of the above tests
($x = 256.193.258) =~ tr/a/b/;
is(length $x, 3);
is($x, 256.193.258);

$x =~ tr/A/B/;
is(length $x, 3);
if (ord("\t") == 9) { # ASCII
    is($x, 256.193.258);
}
else {
    is($x, 256.194.258);
}


{
    my $l = chr(300); my $r = chr(400);
    $x = 200.300.400;
    $x =~ tr/\x{12c}/\x{190}/;
    is($x, 200.400.400,     
                        'changing UTF8 chars in a UTF8 string, same length');
    is(length $x, 3);

    $x = 200.300.400;
    $x =~ tr/\x{12c}/\x{be8}/;
    is($x, 200.3048.400,    '    more bytes');
    is(length $x, 3);

    $x = 100.125.60;
    $x =~ tr/\x{64}/\x{190}/;
    is($x, 400.125.60,      'Putting UT8 chars into a non-UTF8 string');
    is(length $x, 3);

    $x = 400.125.60;
    $x =~ tr/\x{190}/\x{64}/;
    is($x, 100.125.60,      'Removing UTF8 chars from UTF8 string');
    is(length $x, 3);

    $x = 400.125.60.400;
    $y = $x =~ tr/\x{190}/\x{190}/;
    is($y, 2,               'Counting UTF8 chars in UTF8 string');

    $x = 60.400.125.60.400;
    $y = $x =~ tr/\x{3c}/\x{3c}/;
    is($y, 2,               '         non-UTF8 chars in UTF8 string');

    # 17 - counting UTF8 chars in non-UTF8 string
    $x = 200.125.60;
    $y = $x =~ tr/\x{190}/\x{190}/;
    is($y, 0,               '         UTF8 chars in non-UTFs string');
}

$_ = "abcdefghijklmnopqrstuvwxyz";
eval 'tr/a-z-9/ /';
like($@, qr/^Ambiguous range in transliteration operator/,  'tr/a-z-9//');

# 19-21: Make sure leading and trailing hyphens still work
$_ = "car-rot9";
tr/-a-m/./;
is($_, '..r.rot9',  'hyphens, leading');

$_ = "car-rot9";
tr/a-m-/./;
is($_, '..r.rot9',  '   trailing');

$_ = "car-rot9";
tr/-a-m-/./;
is($_, '..r.rot9',  '   both');

$_ = "abcdefghijklmnop";
tr/ae-hn/./;
is($_, '.bcd....ijklm.op');

$_ = "abcdefghijklmnop";
tr/a-cf-kn-p/./;
is($_, '...de......lm...');

$_ = "abcdefghijklmnop";
tr/a-ceg-ikm-o/./;
is($_, '...d.f...j.l...p');


# 20000705 MJD
eval "tr/m-d/ /";
like($@, qr/^Invalid range "m-d" in transliteration operator/,
              'reversed range check');

'abcdef' =~ /(bcd)/;
is(eval '$1 =~ tr/abcd//', 3,  'explicit read-only count');
is($@, '',                      '    no error');

'abcdef' =~ /(bcd)/;
is(eval '$1 =~ tr/abcd/abcd/', 3,  'implicit read-only count');
is($@, '',                      '    no error');

is(eval '"123" =~ tr/12//', 2,     'LHS of non-updating tr');

eval '"123" =~ tr/1/2/';
like($@, qr|^Can't modify constant item in transliteration \(tr///\)|,
         'LHS bad on updating tr');


# v300 (0x12c) is UTF-8-encoded as 196 172 (0xc4 0xac)
# v400 (0x190) is UTF-8-encoded as 198 144 (0xc6 0x90)

# Transliterate a byte to a byte, all four ways.

($a = v300.196.172.300.196.172) =~ tr/\xc4/\xc5/;
is($a, v300.197.172.300.197.172,    'byte2byte transliteration');

($a = v300.196.172.300.196.172) =~ tr/\xc4/\x{c5}/;
is($a, v300.197.172.300.197.172);

($a = v300.196.172.300.196.172) =~ tr/\x{c4}/\xc5/;
is($a, v300.197.172.300.197.172);

($a = v300.196.172.300.196.172) =~ tr/\x{c4}/\x{c5}/;
is($a, v300.197.172.300.197.172);


($a = v300.196.172.300.196.172) =~ tr/\xc4/\x{12d}/;
is($a, v300.301.172.300.301.172,    'byte2wide transliteration');

($a = v300.196.172.300.196.172) =~ tr/\x{12c}/\xc3/;
is($a, v195.196.172.195.196.172,    '   wide2byte');

($a = v300.196.172.300.196.172) =~ tr/\x{12c}/\x{12d}/;
is($a, v301.196.172.301.196.172,    '   wide2wide');


($a = v300.196.172.300.196.172) =~ tr/\xc4\x{12c}/\x{12d}\xc3/;
is($a, v195.301.172.195.301.172,    'byte2wide & wide2byte');


($a = v300.196.172.300.196.172.400.198.144) =~
	tr/\xac\xc4\x{12c}\x{190}/\xad\x{12d}\xc5\x{191}/;
is($a, v197.301.173.197.301.173.401.198.144,    'all together now!');


is((($a = v300.196.172.300.196.172) =~ tr/\xc4/\xc5/), 2,
                                     'transliterate and count');

is((($a = v300.196.172.300.196.172) =~ tr/\x{12c}/\x{12d}/), 2);


($a = v300.196.172.300.196.172) =~ tr/\xc4/\x{12d}/c;
is($a, v301.196.301.301.196.301,    'translit w/complement');

($a = v300.196.172.300.196.172) =~ tr/\x{12c}/\xc5/c;
is($a, v300.197.197.300.197.197);


($a = v300.196.172.300.196.172) =~ tr/\xc4//d;
is($a, v300.172.300.172,            'translit w/deletion');

($a = v300.196.172.300.196.172) =~ tr/\x{12c}//d;
is($a, v196.172.196.172);


($a = v196.196.172.300.300.196.172) =~ tr/\xc4/\xc5/s;
is($a, v197.172.300.300.197.172,    'translit w/squeeze');

($a = v196.172.300.300.196.172.172) =~ tr/\x{12c}/\x{12d}/s;
is($a, v196.172.301.196.172.172);


# Tricky cases (When Simon Cozens Attacks)
($a = v196.172.200) =~ tr/\x{12c}/a/;
is(sprintf("%vd", $a), '196.172.200');

($a = v196.172.200) =~ tr/\x{12c}/\x{12c}/;
is(sprintf("%vd", $a), '196.172.200');

($a = v196.172.200) =~ tr/\x{12c}//d;
is(sprintf("%vd", $a), '196.172.200');


# UTF8 range tests from Inaba Hiroto

# Not working in EBCDIC as of 12674.
($a = v300.196.172.302.197.172) =~ tr/\x{12c}-\x{130}/\xc0-\xc4/;
is($a, v192.196.172.194.197.172,    'UTF range');

($a = v300.196.172.302.197.172) =~ tr/\xc4-\xc8/\x{12c}-\x{130}/;
is($a, v300.300.172.302.301.172);


# UTF8 range tests from Karsten Sperling (patch #9008 required)

($a = "\x{0100}") =~ tr/\x00-\x{100}/X/;
is($a, "X");

($a = "\x{0100}") =~ tr/\x{0000}-\x{00ff}/X/c;
is($a, "X");

($a = "\x{0100}") =~ tr/\x{0000}-\x{00ff}\x{0101}/X/c;
is($a, "X");
 
($a = v256) =~ tr/\x{0000}-\x{00ff}\x{0101}/X/c;
is($a, "X");


# UTF8 range tests from Inaba Hiroto

($a = "\x{200}") =~ tr/\x00-\x{100}/X/c;
is($a, "X");

($a = "\x{200}") =~ tr/\x00-\x{100}/X/cs;
is($a, "X");


# Tricky on EBCDIC: while [a-z] [A-Z] must not match the gap characters,
# (i-j, r-s, I-J, R-S), [\x89-\x91] [\xc9-\xd1] has to match them,
# from Karsten Sperling.

$c = ($a = "\x89\x8a\x8b\x8c\x8d\x8f\x90\x91") =~ tr/\x89-\x91/X/;
is($c, 8);
is($a, "XXXXXXXX");

$c = ($a = "\xc9\xca\xcb\xcc\xcd\xcf\xd0\xd1") =~ tr/\xc9-\xd1/X/;
is($c, 8);
is($a, "XXXXXXXX");

SKIP: {
    skip "not EBCDIC", 4 unless $Is_EBCDIC;

    $c = ($a = "\x89\x8a\x8b\x8c\x8d\x8f\x90\x91") =~ tr/i-j/X/;
    is($c, 2);
    is($a, "X\x8a\x8b\x8c\x8d\x8f\x90X");
   
    $c = ($a = "\xc9\xca\xcb\xcc\xcd\xcf\xd0\xd1") =~ tr/I-J/X/;
    is($c, 2);
    is($a, "X\xca\xcb\xcc\xcd\xcf\xd0X");
}

($a = "\x{100}") =~ tr/\x00-\xff/X/c;
is(ord($a), ord("X"));

($a = "\x{100}") =~ tr/\x00-\xff/X/cs;
is(ord($a), ord("X"));

($a = "\x{100}\x{100}") =~ tr/\x{101}-\x{200}//c;
is($a, "\x{100}\x{100}");

($a = "\x{100}\x{100}") =~ tr/\x{101}-\x{200}//cs;
is($a, "\x{100}");

$a = "\xfe\xff"; $a =~ tr/\xfe\xff/\x{1ff}\x{1fe}/;
is($a, "\x{1ff}\x{1fe}");


# From David Dyck
($a = "R0_001") =~ tr/R_//d;
is(hex($a), 1);

# From Inaba Hiroto
@a = (1,2); map { y/1/./ for $_ } @a;
is("@a", ". 2");

@a = (1,2); map { y/1/./ for $_.'' } @a;
is("@a", "1 2");


# Additional test for Inaba Hiroto patch (robin@kitsite.com)
($a = "\x{100}\x{102}\x{101}") =~ tr/\x00-\377/XYZ/c;
is($a, "XZY");


# Used to fail with "Modification of a read-only value attempted"
%a = (N=>1);
foreach (keys %a) {
  eval 'tr/N/n/';
  is($_, 'n',   'pp_trans needs to unshare shared hash keys');
  is($@, '',    '   no error');
}


$x = eval '"1213" =~ tr/1/1/';
is($x, 2,   'implicit count on constant');
is($@, '',  '   no error');


my @foo = ();
eval '$foo[-1] =~ tr/N/N/';
is( $@, '',         'implicit count outside array bounds, index negative' );
is( scalar @foo, 0, "    doesn't extend the array");

eval '$foo[1] =~ tr/N/N/';
is( $@, '',         'implicit count outside array bounds, index positive' );
is( scalar @foo, 0, "    doesn't extend the array");


my %foo = ();
eval '$foo{bar} =~ tr/N/N/';
is( $@, '',         'implicit count outside hash bounds' );
is( scalar keys %foo, 0,   "    doesn't extend the hash");

$x = \"foo";
is( $x =~ tr/A/A/, 2, 'non-modifying tr/// on a scalar ref' );
is( ref $x, 'SCALAR', "    doesn't stringify its argument" );

# rt.perl.org 36622.  Perl didn't like a y/// at end of file.  No trailing
# newline allowed.
fresh_perl_is(q[$_ = "foo"; y/A-Z/a-z/], '');


{ # [perl #38293] chr(65535) should be allowed in regexes
no warnings 'utf8'; # to allow non-characters

$s = "\x{d800}\x{ffff}";
$s =~ tr/\0/A/;
is($s, "\x{d800}\x{ffff}", "do_trans_simple");

$s = "\x{d800}\x{ffff}";
$i = $s =~ tr/\0//;
is($i, 0, "do_trans_count");

$s = "\x{d800}\x{ffff}";
$s =~ tr/\0/A/s;
is($s, "\x{d800}\x{ffff}", "do_trans_complex, SQUASH");

$s = "\x{d800}\x{ffff}";
$s =~ tr/\0/A/c;
is($s, "AA", "do_trans_complex, COMPLEMENT");

$s = "A\x{ffff}B";
$s =~ tr/\x{ffff}/\x{1ffff}/;
is($s, "A\x{1ffff}B", "utf8, SEARCHLIST");

$s = "\x{fffd}\x{fffe}\x{ffff}";
$s =~ tr/\x{fffd}-\x{ffff}/ABC/;
is($s, "ABC", "utf8, SEARCHLIST range");

$s = "ABC";
$s =~ tr/ABC/\x{ffff}/;
is($s, "\x{ffff}"x3, "utf8, REPLACEMENTLIST");

$s = "ABC";
$s =~ tr/ABC/\x{fffd}-\x{ffff}/;
is($s, "\x{fffd}\x{fffe}\x{ffff}", "utf8, REPLACEMENTLIST range");

$s = "A\x{ffff}B\x{100}\0\x{fffe}\x{ffff}";
$i = $s =~ tr/\x{ffff}//;
is($i, 2, "utf8, count");

$s = "A\x{ffff}\x{ffff}C";
$s =~ tr/\x{ffff}/\x{100}/s;
is($s, "A\x{100}C", "utf8, SQUASH");

$s = "A\x{ffff}\x{ffff}\x{fffe}\x{fffe}\x{fffe}C";
$s =~ tr/\x{fffe}\x{ffff}//s;
is($s, "A\x{ffff}\x{fffe}C", "utf8, SQUASH");

$s = "xAABBBy";
$s =~ tr/AB/\x{ffff}/s;
is($s, "x\x{ffff}y", "utf8, SQUASH");

$s = "xAABBBy";
$s =~ tr/AB/\x{fffe}\x{ffff}/s;
is($s, "x\x{fffe}\x{ffff}y", "utf8, SQUASH");

$s = "A\x{ffff}B\x{fffe}C";
$s =~ tr/\x{fffe}\x{ffff}/x/c;
is($s, "x\x{ffff}x\x{fffe}x", "utf8, COMPLEMENT");

$s = "A\x{10000}B\x{2abcd}C";
$s =~ tr/\0-\x{ffff}/x/c;
is($s, "AxBxC", "utf8, COMPLEMENT range");

$s = "A\x{fffe}B\x{ffff}C";
$s =~ tr/\x{fffe}\x{ffff}/x/d;
is($s, "AxBC", "utf8, DELETE");

} # non-characters end

{ # related to [perl #27940]
    my $c;

    ($c = "\x20\c@\x30\cA\x40\cZ\x50\c_\x60") =~ tr/\c@-\c_//d;
    is($c, "\x20\x30\x40\x50\x60", "tr/\\c\@-\\c_//d");

    ($c = "\x20\x00\x30\x01\x40\x1A\x50\x1F\x60") =~ tr/\x00-\x1f//d;
    is($c, "\x20\x30\x40\x50\x60", "tr/\\x00-\\x1f//d");
}

($s) = keys %{{pie => 3}};
SKIP: {
    if (!eval { require XS::APItest }) { skip "no XS::APItest", 2 }
    my $wasro = XS::APItest::SvIsCOW($s);
    ok $wasro, "have a COW";
    $s =~ tr/i//;
    ok( XS::APItest::SvIsCOW($s),
       "count-only tr doesn't deCOW COWs" );
}

# [ RT #61520 ]
#
# under threads, unicode tr within a cloned closure would SEGV or assert
# fail, since the pointer in the pad to the swash was getting zeroed out
# in the proto-CV

{
    my $x = "\x{142}";
    sub {
	$x =~ tr[\x{142}][\x{143}];
    }->();
    is($x,"\x{143}", "utf8 + closure");
}

# Freeing of trans ops prior to pmtrans() [perl #102858].
eval q{ $a ~= tr/a/b/; };
ok 1;
SKIP: {
    no warnings "deprecated";
    skip "no encoding", 1 unless eval { require encoding; 1 };
    eval q{ use encoding "utf8"; $a ~= tr/a/b/; };
    ok 1;
}

{ # [perl #113584]

    my $x = "Perlα";
    $x =~ tr/αα/βγ/;
    note $x;
    is($x, "Perlβ", "Only first of multiple transliterations is used");
}

# tr/a/b/ should fail even on zero-length read-only strings
use constant nullrocow => (keys%{{""=>undef}})[0];
for ("", nullrocow) {
    eval { $_ =~ y/a/b/ };
    like $@, qr/^Modification of a read-only value attempted at /,
        'tr/a/b/ fails on zero-length ro string';
}

1;
