#!./perl

#
# test the bit operators '&', '|', '^', '~', '<<', and '>>'
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require "./test.pl";
    require Config;
}

# Tests don't have names yet.
# If you find tests are failing, please try adding names to tests to track
# down where the failure is, and supply your new names as a patch.
# (Just-in-time test naming)
plan tests => 161 + (10*13*2) + 4;

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
my $a = v120.300;
my $b = v200.400;
$a ^= $b;
is (sprintf("%vd", $a), '176.188');
my $a = v120.300;
my $b = v200.400;
$a |= $b;
is (sprintf("%vd", $a), '248.444');

#
# UTF8 ~ behaviour
#

my $Is_EBCDIC = (ord('A') == 193) ? 1 : 0;

my @not36;

for (0x100...0xFFF) {
  $a = ~(chr $_);
  if ($Is_EBCDIC) {
      push @not36, sprintf("%#03X", $_)
          if $a ne chr(~$_) or length($a) != 1;
  }
  else {
      push @not36, sprintf("%#03X", $_)
          if $a ne chr(~$_) or length($a) != 1 or ~$a ne chr($_);
  }
}
is (join (', ', @not36), '');

my @not37;

for my $i (0xEEE...0xF00) {
  for my $j (0x0..0x120) {
    $a = ~(chr ($i) . chr $j);
    if ($Is_EBCDIC) {
        push @not37, sprintf("%#03X %#03X", $i, $j)
	    if $a ne chr(~$i).chr(~$j) or
	       length($a) != 2;
    }
    else {
        push @not37, sprintf("%#03X %#03X", $i, $j)
	    if $a ne chr(~$i).chr(~$j) or
	       length($a) != 2 or 
               ~$a ne chr($i).chr($j);
    }
  }
}
is (join (', ', @not37), '');

SKIP: {
  skip "EBCDIC" if $Is_EBCDIC;
  is (~chr(~0), "\0");
}


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
  like( runperl(prog => 'eval q($#a>>=1); print 1'), "^1\n?" );
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

# update to pp_complement() via Coverity
SKIP: {
  # UTF-EBCDIC is limited to 0x7fffffff and can't encode ~0.
  skip "EBCDIC" if $Is_EBCDIC;

  my $str = "\x{10000}\x{800}";
  # U+10000 is four bytes in UTF-8/UTF-EBCDIC.
  # U+0800 is three bytes in UTF-8/UTF-EBCDIC.

  no warnings "utf8";
  { use bytes; $str =~ s/\C\C\z//; }

  # it's really bogus that (~~malformed) is \0.
  my $ref = "\x{10000}\0";
  is(~~$str, $ref);
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
index "foo", PVBM;

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

ok(!eval { bless([], "Bar") | "x"; 1 },     "string overload can't use |");
like($@, qr/no method found/,               "correct error");
is(eval { bless([], "Baz") | "x" }, "y",    "| overload works");

my $obj = bless [], "Bar";
$strval = "x";
eval { $obj |= "Q" };
$strval = "z";
is("$obj", "z", "|= doesn't break string overload");
