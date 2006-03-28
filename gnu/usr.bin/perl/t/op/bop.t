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
plan tests => 49;

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
