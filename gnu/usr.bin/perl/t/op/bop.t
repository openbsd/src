#!./perl

#
# test the bit operators '&', '|', '^', '~', '<<', and '>>'
#

use warnings;
no warnings 'deprecated';

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require "./test.pl"; require "./charset_tools.pl";
    require Config;
}

# Tests don't have names yet.
# If you find tests are failing, please try adding names to tests to track
# down where the failure is, and supply your new names as a patch.
# (Just-in-time test naming)
plan tests => 192 + (10*13*2) + 5 + 29;

# numerics
ok ((0xdead & 0xbeef) == 0x9ead);
ok ((0xdead | 0xbeef) == 0xfeef);
ok ((0xdead ^ 0xbeef) == 0x6042);
ok ((~0xdead & 0xbeef) == 0x2042);

# shifts
ok ((257 << 7) == 32896);
ok ((33023 >> 7) == 257);

# signed vs. unsigned
ok ((~0 > 0 && do { use integer; ~0 } == -1));

my $bits = 0;
for (my $i = ~0; $i; $i >>= 1) { ++$bits; }
my $cusp = 1 << ($bits - 1);


ok (($cusp & -1) > 0 && do { use integer; $cusp & -1 } < 0);
ok (($cusp | 1) > 0 && do { use integer; $cusp | 1 } < 0);
ok (($cusp ^ 1) > 0 && do { use integer; $cusp ^ 1 } < 0);
ok ((1 << ($bits - 1)) == $cusp &&
    do { use integer; 1 << ($bits - 1) } == -$cusp);
ok (($cusp >> 1) == ($cusp / 2) &&
    do { use integer; abs($cusp >> 1) } == ($cusp / 2));

$Aaz = chr(ord("A") & ord("z"));
$Aoz = chr(ord("A") | ord("z"));
$Axz = chr(ord("A") ^ ord("z"));

# short strings
is (("AAAAA" & "zzzzz"), ($Aaz x 5));
is (("AAAAA" | "zzzzz"), ($Aoz x 5));
is (("AAAAA" ^ "zzzzz"), ($Axz x 5));

# long strings
$foo = "A" x 150;
$bar = "z" x 75;
$zap = "A" x 75;
# & truncates
is (($foo & $bar), ($Aaz x 75 ));
# | does not truncate
is (($foo | $bar), ($Aoz x 75 . $zap));
# ^ does not truncate
is (($foo ^ $bar), ($Axz x 75 . $zap));

# string constants.  These tests expect the bit patterns of these strings in
# ASCII, so convert to that.
sub _and($) { $_[0] & native_to_uni("+0") }
sub _oar($) { $_[0] | native_to_uni("+0") }
sub _xor($) { $_[0] ^ native_to_uni("+0") }
is _and native_to_uni("waf"), native_to_uni('# '),  'str var & const str'; # [perl #20661]
is _and native_to_uni("waf"), native_to_uni('# '),  'str var & const str again'; # [perl #20661]
is _oar native_to_uni("yit"), native_to_uni('{yt'), 'str var | const str';
is _oar native_to_uni("yit"), native_to_uni('{yt'), 'str var | const str again';
is _xor native_to_uni("yit"), native_to_uni('RYt'), 'str var ^ const str';
is _xor native_to_uni("yit"), native_to_uni('RYt'), 'str var ^ const str again';

SKIP: {
    skip "Converting a numeric doesn't work with EBCDIC unlike the above tests",
         3 if $::IS_EBCDIC;
    is _and  0, '0',   'num var & const str';     # [perl #20661]
    is _oar  0, '0',   'num var | const str';
    is _xor  0, '0',   'num var ^ const str';
}

# But don’t mistake a COW for a constant when assigning to it
%h=(150=>1);
$i=(keys %h)[0];
$i |= 105;
is $i, 255, '[perl #108480] $cow |= number';
$i=(keys %h)[0];
$i &= 105;
is $i, 0, '[perl #108480] $cow &= number';
$i=(keys %h)[0];
$i ^= 105;
is $i, 255, '[perl #108480] $cow ^= number';

#
is ("ok \xFF\xFF\n" & "ok 19\n", "ok 19\n");
is ("ok 20\n" | "ok \0\0\n", "ok 20\n");
is ("o\000 \0001\000" ^ "\000k\0002\000\n", "ok 21\n");

#
is ("ok \x{FF}\x{FF}\n" & "ok 22\n", "ok 22\n");
is ("ok 23\n" | "ok \x{0}\x{0}\n", "ok 23\n");
is ("o\x{0} \x{0}4\x{0}" ^ "\x{0}k\x{0}2\x{0}\n", "ok 24\n");

#
is (sprintf("%vd", v4095 & v801), 801);
is (sprintf("%vd", v4095 | v801), 4095);
is (sprintf("%vd", v4095 ^ v801), 3294);

#
is (sprintf("%vd", v4095.801.4095 & v801.4095), '801.801');
is (sprintf("%vd", v4095.801.4095 | v801.4095), '4095.4095.4095');
is (sprintf("%vd", v801.4095 ^ v4095.801.4095), '3294.3294.4095');
#
is (sprintf("%vd", v120.300 & v200.400), '72.256');
is (sprintf("%vd", v120.300 | v200.400), '248.444');
is (sprintf("%vd", v120.300 ^ v200.400), '176.188');
#
{
    my $a = v120.300;
    my $b = v200.400;
    $a ^= $b;
    is (sprintf("%vd", $a), '176.188');
}
{
    my $a = v120.300;
    my $b = v200.400;
    $a |= $b;
    is (sprintf("%vd", $a), '248.444');
}

#
# UTF8 ~ behaviour
#

{
    my @not36;

    for (0x100...0xFFF) {
    $a = ~(chr $_);
        push @not36, sprintf("%#03X", $_)
            if $a ne chr(~$_) or length($a) != 1 or ~$a ne chr($_);
    }
    is (join (', ', @not36), '');

    my @not37;

    for my $i (0xEEE...0xF00) {
        for my $j (0x0..0x120) {
            $a = ~(chr ($i) . chr $j);
                push @not37, sprintf("%#03X %#03X", $i, $j)
                    if $a ne chr(~$i).chr(~$j) or
                    length($a) != 2 or
                    ~$a ne chr($i).chr($j);
        }
    }
    is (join (', ', @not37), '');

    is (~chr(~0), "\0");


    my @not39;

    for my $i (0x100..0x120) {
        for my $j (0x100...0x120) {
            push @not39, sprintf("%#03X %#03X", $i, $j)
                if ~(chr($i)|chr($j)) ne (~chr($i)&~chr($j));
        }
    }
    is (join (', ', @not39), '');

    my @not40;

    for my $i (0x100..0x120) {
        for my $j (0x100...0x120) {
            push @not40, sprintf("%#03X %#03X", $i, $j)
                if ~(chr($i)&chr($j)) ne (~chr($i)|~chr($j));
        }
    }
    is (join (', ', @not40), '');
}


# More variations on 19 and 22.
is ("ok \xFF\x{FF}\n" & "ok 41\n", "ok 41\n");
is ("ok \x{FF}\xFF\n" & "ok 42\n", "ok 42\n");

# Tests to see if you really can do casts negative floats to unsigned properly
$neg1 = -1.0;
ok (~ $neg1 == 0);
$neg7 = -7.0;
ok (~ $neg7 == 6);


# double magic tests

sub TIESCALAR { bless { value => $_[1], orig => $_[1] } }
sub STORE { $_[0]{store}++; $_[0]{value} = $_[1] }
sub FETCH { $_[0]{fetch}++; $_[0]{value} }
sub stores { tied($_[0])->{value} = tied($_[0])->{orig};
             delete(tied($_[0])->{store}) || 0 }
sub fetches { delete(tied($_[0])->{fetch}) || 0 }

# numeric double magic tests

tie $x, "main", 1;
tie $y, "main", 3;

is(($x | $y), 3);
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x & $y), 1);
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x ^ $y), 2);
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x |= $y), 3);
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(($x &= $y), 1);
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(($x ^= $y), 2);
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(~~$y, 3);
is(fetches($y), 1);
is(stores($y), 0);

{ use integer;

is(($x | $y), 3);
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x & $y), 1);
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x ^ $y), 2);
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x |= $y), 3);
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(($x &= $y), 1);
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(($x ^= $y), 2);
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(~$y, -4);
is(fetches($y), 1);
is(stores($y), 0);

} # end of use integer;

# stringwise double magic tests

tie $x, "main", "a";
tie $y, "main", "c";

is(($x | $y), ("a" | "c"));
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x & $y), ("a" & "c"));
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x ^ $y), ("a" ^ "c"));
is(fetches($x), 1);
is(fetches($y), 1);
is(stores($x), 0);
is(stores($y), 0);

is(($x |= $y), ("a" | "c"));
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(($x &= $y), ("a" & "c"));
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(($x ^= $y), ("a" ^ "c"));
is(fetches($x), 2);
is(fetches($y), 1);
is(stores($x), 1);
is(stores($y), 0);

is(~~$y, "c");
is(fetches($y), 1);
is(stores($y), 0);

$a = "\0\x{100}"; chop($a);
ok(utf8::is_utf8($a)); # make sure UTF8 flag is still there
$a = ~$a;
is($a, "\xFF", "~ works with utf-8");

# [rt.perl.org 33003]
# This would cause a segfault without malloc wrap
SKIP: {
  skip "No malloc wrap checks" unless $Config::Config{usemallocwrap};
  like( runperl(prog => 'eval q($#a>>=1); print 1'), qr/^1\n?/ );
}

# [perl #37616] Bug in &= (string) and/or m//
{
    $a = "aa";
    $a &= "a";
    ok($a =~ /a+$/, 'ASCII "a" is NUL-terminated');

    $b = "bb\x{100}";
    $b &= "b";
    ok($b =~ /b+$/, 'Unicode "b" is NUL-terminated');
}

{
    $a = chr(0x101) x 0x101;
    $b = chr(0x0FF) x 0x0FF;

    $c = $a | $b;
    is($c, chr(0x1FF) x 0xFF . chr(0x101) x 2);

    $c = $b | $a;
    is($c, chr(0x1FF) x 0xFF . chr(0x101) x 2);

    $c = $a & $b;
    is($c, chr(0x001) x 0x0FF);

    $c = $b & $a;
    is($c, chr(0x001) x 0x0FF);

    $c = $a ^ $b;
    is($c, chr(0x1FE) x 0x0FF . chr(0x101) x 2);

    $c = $b ^ $a;
    is($c, chr(0x1FE) x 0x0FF . chr(0x101) x 2);
}

{
    $a = chr(0x101) x 0x101;
    $b = chr(0x0FF) x 0x0FF;

    $a |= $b;
    is($a, chr(0x1FF) x 0xFF . chr(0x101) x 2);
}

{
    $a = chr(0x101) x 0x101;
    $b = chr(0x0FF) x 0x0FF;

    $b |= $a;
    is($b, chr(0x1FF) x 0xFF . chr(0x101) x 2);
}

{
    $a = chr(0x101) x 0x101;
    $b = chr(0x0FF) x 0x0FF;

    $a &= $b;
    is($a, chr(0x001) x 0x0FF);
}

{
    $a = chr(0x101) x 0x101;
    $b = chr(0x0FF) x 0x0FF;

    $b &= $a;
    is($b, chr(0x001) x 0x0FF);
}

{
    $a = chr(0x101) x 0x101;
    $b = chr(0x0FF) x 0x0FF;

    $a ^= $b;
    is($a, chr(0x1FE) x 0x0FF . chr(0x101) x 2);
}

{
    $a = chr(0x101) x 0x101;
    $b = chr(0x0FF) x 0x0FF;

    $b ^= $a;
    is($b, chr(0x1FE) x 0x0FF . chr(0x101) x 2);
}


# New string- and number-specific bitwise ops
{
  use feature "bitwise";
  no warnings "experimental::bitwise";
  is "22" & "66", 2,    'numeric & with strings';
  is "22" | "66", 86,   'numeric | with strings';
  is "22" ^ "66", 84,   'numeric ^ with strings';
  is ~"22" & 0xff, 233, 'numeric ~ with string';
  is 22 &. 66, 22,     '&. with numbers';
  is 22 |. 66, 66,     '|. with numbers';
  is 22 ^. 66, "\4\4", '^. with numbers';
  if ($::IS_EBCDIC) {
    # ord('2') is 0xF2 on EBCDIC
    is ~.22, "\x0d\x0d", '~. with number';
  }
  else {
    # ord('2') is 0x32 on ASCII
    is ~.22, "\xcd\xcd", '~. with number';
  }
  $_ = "22";
  is $_ &= "66", 2,  'numeric &= with strings';
  $_ = "22";
  is $_ |= "66", 86, 'numeric |= with strings';
  $_ = "22";
  is $_ ^= "66", 84, 'numeric ^= with strings';
  $_ = 22;
  is $_ &.= 66, 22,     '&.= with numbers';
  $_ = 22;
  is $_ |.= 66, 66,     '|.= with numbers';
  $_ = 22;
  is $_ ^.= 66, "\4\4", '^.= with numbers';

 # signed vs. unsigned
 ok ((~0 > 0 && do { use integer; ~0 } == -1));

 my $bits = 0;
 for (my $i = ~0; $i; $i >>= 1) { ++$bits; }
 my $cusp = 1 << ($bits - 1);

 ok (($cusp & -1) > 0 && do { use integer; $cusp & -1 } < 0);
 ok (($cusp | 1) > 0 && do { use integer; $cusp | 1 } < 0);
 ok (($cusp ^ 1) > 0 && do { use integer; $cusp ^ 1 } < 0);
 ok ((1 << ($bits - 1)) == $cusp &&
     do { use integer; 1 << ($bits - 1) } == -$cusp);
 ok (($cusp >> 1) == ($cusp / 2) &&
    do { use integer; abs($cusp >> 1) } == ($cusp / 2));
}

# ref tests

my %res;

for my $str ("x", "\x{100}") {
    for my $chr (qw/S A H G X ( * F/) {
        for my $op (qw/| & ^/) {
            my $co = ord $chr;
            my $so = ord $str;
            $res{"$chr$op$str"} = eval qq/chr($co $op $so)/;
        }
    }
    $res{"undef|$str"} = $str;
    $res{"undef&$str"} = "";
    $res{"undef^$str"} = $str;
}

sub PVBM () { "X" }
1 if index "foo", PVBM;

my $warn = 0;
local $^W = 1;
local $SIG{__WARN__} = sub { $warn++ };

sub is_first {
    my ($got, $orig, $op, $str, $name) = @_;
    is(substr($got, 0, 1), $res{"$orig$op$str"}, $name);
}

for (
    # [object to test, first char of stringification, name]
    [undef,             "undef",    "undef"         ],
    [\1,                "S",        "scalar ref"    ],
    [[],                "A",        "array ref"     ],
    [{},                "H",        "hash ref"      ],
    [qr/x/,             "(",        "qr//"          ],
    [*foo,              "*",        "glob"          ],
    [\*foo,             "G",        "glob ref"      ],
    [PVBM,              "X",        "PVBM"          ],
    [\PVBM,             "S",        "PVBM ref"      ],
    [bless([], "Foo"),  "F",        "object"        ],
) {
    my ($val, $orig, $type) = @$_;

    for (["x", "string"], ["\x{100}", "utf8"]) {
        my ($str, $desc) = @$_;

        $warn = 0;

        is_first($val | $str, $orig, "|", $str, "$type | $desc");
        is_first($val & $str, $orig, "&", $str, "$type & $desc");
        is_first($val ^ $str, $orig, "^", $str, "$type ^ $desc");

        is_first($str | $val, $orig, "|", $str, "$desc | $type");
        is_first($str & $val, $orig, "&", $str, "$desc & $type");
        is_first($str ^ $val, $orig, "^", $str, "$desc ^ $type");

        my $new;
        ($new = $val) |= $str;
        is_first($new, $orig, "|", $str, "$type |= $desc");
        ($new = $val) &= $str;
        is_first($new, $orig, "&", $str, "$type &= $desc");
        ($new = $val) ^= $str;
        is_first($new, $orig, "^", $str, "$type ^= $desc");

        ($new = $str) |= $val;
        is_first($new, $orig, "|", $str, "$desc |= $type");
        ($new = $str) &= $val;
        is_first($new, $orig, "&", $str, "$desc &= $type");
        ($new = $str) ^= $val;
        is_first($new, $orig, "^", $str, "$desc ^= $type");

        if ($orig eq "undef") {
            # undef |= and undef ^= don't warn
            is($warn, 10, "no duplicate warnings");
        }
        else {
            is($warn, 0, "no warnings");
        }
    }
}

my $strval;

{
    package Bar;
    use overload q/""/ => sub { $strval };

    package Baz;
    use overload q/|/ => sub { "y" };
}

ok(!eval { 1 if bless([], "Bar") | "x"; 1 },"string overload can't use |");
like($@, qr/no method found/,               "correct error");
is(eval { bless([], "Baz") | "x" }, "y",    "| overload works");

my $obj = bless [], "Bar";
$strval = "x";
eval { $obj |= "Q" };
$strval = "z";
is("$obj", "z", "|= doesn't break string overload");

# [perl #29070]
$^A .= new version ~$_ for eval sprintf('"\\x%02x"', 0xff - ord("1")),
                           $::IS_EBCDIC ? v13 : v205, # 255 - ord('2')
                           eval sprintf('"\\x%02x"', 0xff - ord("3"));
is $^A, "123", '~v0 clears vstring magic on retval';

{
    my $w = $Config::Config{ivsize} * 8;

    fail("unexpected w $w") unless $w == 32 || $w == 64;

    is(1 << 1, 2, "UV 1 left shift 1");
    is(1 >> 1, 0, "UV 1 right shift 1");

    is(0x7b << -4, 0x007, "UV left negative shift == right shift");
    is(0x7b >> -4, 0x7b0, "UV right negative shift == left shift");

    is(0x7b <<  0, 0x07b, "UV left  zero shift == identity");
    is(0x7b >>  0, 0x07b, "UV right zero shift == identity");

    is(0x0 << -1, 0x0, "zero left  negative shift == zero");
    is(0x0 >> -1, 0x0, "zero right negative shift == zero");

    cmp_ok(1 << $w - 1, '==', 2 ** ($w - 1), # not is() because NV stringify.
       "UV left $w - 1 shift == 2 ** ($w - 1)");
    is(1 << $w,     0, "UV left shift $w     == zero");
    is(1 << $w + 1, 0, "UV left shift $w + 1 == zero");

    is(1 >> $w - 1, 0, "UV right shift $w - 1 == zero");
    is(1 >> $w,     0, "UV right shift $w     == zero");
    is(1 >> $w + 1, 0, "UV right shift $w + 1 == zero");

    # Negative shiftees get promoted to UVs before shifting.  This is
    # not necessarily the ideal behavior, but that is what is happening.
    if ($w == 64) {
        no warnings "portable";
        no warnings "overflow"; # prevent compile-time warning for ivsize=4
        is(-1 << 1, 0xFFFF_FFFF_FFFF_FFFE,
           "neg UV (sic) left shift  = 0xFF..E");
        is(-1 >> 1, 0x7FFF_FFFF_FFFF_FFFF,
           "neg UV (sic) right right = 0x7F..F");
    } elsif ($w == 32) {
        no warnings "portable";
        is(-1 << 1, 0xFFFF_FFFE, "neg left shift  == 0xFF..E");
        is(-1 >> 1, 0x7FFF_FFFF, "neg right right == 0x7F..F");
    }

    {
        # 'use integer' means use IVs instead of UVs.
        use integer;

        # No surprises here.
        is(1 << 1, 2, "IV 1 left shift 1  == 2");
        is(1 >> 1, 0, "IV 1 right shift 1 == 0");

        # The left overshift should behave like without 'use integer',
        # that is, return zero.
        is(1 << $w,     0, "IV 1 left shift $w     == 0");
        is(1 << $w + 1, 0, "IV 1 left shift $w + 1 == 0");
        is(-1 << $w,     0, "IV -1 left shift $w     == 0");
        is(-1 << $w + 1, 0, "IV -1 left shift $w + 1 == 0");

        # Even for negative IVs, left shift is multiplication.
        # But right shift should display the stuckiness to -1.
        is(-1 <<      1, -2, "IV -1 left shift       1 == -2");
        is(-1 >>      1, -1, "IV -1 right shift      1 == -1");

        # As for UVs, negative shifting means the reverse shift.
        is(-1 <<     -1, -1, "IV -1 left shift      -1 == -1");
        is(-1 >>     -1, -2, "IV -1 right shift     -1 == -2");

        # Test also at and around wordsize, expect stuckiness to -1.
        is(-1 >> $w - 1, -1, "IV -1 right shift $w - 1 == -1");
        is(-1 >> $w,     -1, "IV -1 right shift $w     == -1");
        is(-1 >> $w + 1, -1, "IV -1 right shift $w + 1 == -1");
    }
}
