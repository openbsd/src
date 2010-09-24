#./perl

BEGIN {
	eval { my $q = pack "q", 0 };
	if ($@) {
		print "1..0 # Skip: no 64-bit types\n";
		exit(0);
	}
	chdir 't' if -d 't';
	@INC = '../lib';
}

# This could use many more tests.

# so that using > 0xfffffff constants and
# 32+ bit integers don't cause noise
use warnings;
no warnings qw(overflow portable);

print "1..67\n";

# as 6 * 6 = 36, the last digit of 6**n will always be six. Hence the last
# digit of 16**n will always be six. Hence 16**n - 1 will always end in 5.
# Assumption is that UVs will always be a multiple of 4 bits long.

my $UV_max = ~0;
die "UV_max eq '$UV_max', doesn't end in 5; your UV isn't 4n bits long :-(."
  unless $UV_max =~ /5$/;
my $UV_max_less3 = $UV_max - 3;
my $maths_preserves_UVs = $UV_max_less3 =~ /^\d+2$/;   # 5 - 3 is 2.
if ($maths_preserves_UVs) {
  print "# This perl's maths preserves all bits of a UV.\n";
} else {
  print "# This perl's maths does not preserve all bits of a UV.\n";
}

my $q = 12345678901;
my $r = 23456789012;
my $f = 0xffffffff;
my $x;
my $y;

$x = unpack "q", pack "q", $q;
print "not " unless $x == $q && $x > $f;
print "ok 1\n";


$x = sprintf("%lld", 12345678901);
print "not " unless $x eq $q && $x > $f;
print "ok 2\n";


$x = sprintf("%lld", $q);
print "not " unless $x == $q && $x eq $q && $x > $f;
print "ok 3\n";

$x = sprintf("%Ld", $q);
print "not " unless $x == $q && $x eq $q && $x > $f;
print "ok 4\n";

$x = sprintf("%qd", $q);
print "not " unless $x == $q && $x eq $q && $x > $f;
print "ok 5\n";


$x = sprintf("%llx", $q);
print "not " unless hex($x) == 0x2dfdc1c35 && hex($x) > $f;
print "ok 6\n";

$x = sprintf("%Lx", $q);
print "not " unless hex($x) == 0x2dfdc1c35 && hex($x) > $f;
print "ok 7\n";

$x = sprintf("%qx", $q);
print "not " unless hex($x) == 0x2dfdc1c35 && hex($x) > $f;
print "ok 8\n";


$x = sprintf("%llo", $q);
print "not " unless oct("0$x") == 0133767016065 && oct($x) > $f;
print "ok 9\n";

$x = sprintf("%Lo", $q);
print "not " unless oct("0$x") == 0133767016065 && oct($x) > $f;
print "ok 10\n";

$x = sprintf("%qo", $q);
print "not " unless oct("0$x") == 0133767016065 && oct($x) > $f;
print "ok 11\n";


$x = sprintf("%llb", $q);
print "not " unless oct("0b$x") == 0b1011011111110111000001110000110101 &&
                    oct("0b$x") > $f;
print "ok 12\n";

$x = sprintf("%Lb", $q);
print "not " unless oct("0b$x") == 0b1011011111110111000001110000110101 &&
                                   oct("0b$x") > $f;
print "ok 13\n";

$x = sprintf("%qb", $q);
print "not " unless oct("0b$x") == 0b1011011111110111000001110000110101 &&
                    oct("0b$x") > $f;
print "ok 14\n";


$x = sprintf("%llu", $q);
print "not " unless $x eq $q && $x > $f;
print "ok 15\n";

$x = sprintf("%Lu", $q);
print "not " unless $x == $q && $x eq $q && $x > $f;
print "ok 16\n";

$x = sprintf("%qu", $q);
print "not " unless $x == $q && $x eq $q && $x > $f;
print "ok 17\n";


$x = sprintf("%D", $q);
print "not " unless $x == $q && $x eq $q && $x > $f;
print "ok 18\n";

$x = sprintf("%U", $q);
print "not " unless $x == $q && $x eq $q && $x > $f;
print "ok 19\n";

$x = sprintf("%O", $q);
print "not " unless oct($x) == $q && oct($x) > $f;
print "ok 20\n";


$x = $q + $r;
print "not " unless $x == 35802467913 && $x > $f;
print "ok 21\n";

$x = $q - $r;
print "not " unless $x == -11111110111 && -$x > $f;
print "ok 22\n";

if ($^O ne 'unicos') {
    $x = $q * 1234567;
    print "not " unless $x == 15241567763770867 && $x > $f;
    print "ok 23\n";

    $x /= 1234567;
    print "not " unless $x == $q && $x > $f;
    print "ok 24\n";

    $x = 98765432109 % 12345678901;
    print "not " unless $x == 901;
    print "ok 25\n";
    
    # The following 12 tests adapted from op/inc.

    $a = 9223372036854775807;
    $c = $a++;
    print "not " unless $a == 9223372036854775808;
    print "ok 26\n";

    $a = 9223372036854775807;
    $c = ++$a;
    print "not "
	unless $a == 9223372036854775808 && $c == $a;
    print "ok 27\n";

    $a = 9223372036854775807;
    $c = $a + 1;
    print "not "
	unless $a == 9223372036854775807 && $c == 9223372036854775808;
    print "ok 28\n";

    $a = -9223372036854775808;
    {
	no warnings 'imprecision';
	$c = $a--;
    }
    print "not "
	unless $a == -9223372036854775809 && $c == -9223372036854775808;
    print "ok 29\n";

    $a = -9223372036854775808;
    {
	no warnings 'imprecision';
	$c = --$a;
    }
    print "not "
	unless $a == -9223372036854775809 && $c == $a;
    print "ok 30\n";

    $a = -9223372036854775808;
    $c = $a - 1;
    print "not "
	unless $a == -9223372036854775808 && $c == -9223372036854775809;
    print "ok 31\n";
    
    $a = 9223372036854775808;
    $a = -$a;
    {
	no warnings 'imprecision';
	$c = $a--;
    }
    print "not "
	unless $a == -9223372036854775809 && $c == -9223372036854775808;
    print "ok 32\n";
    
    $a = 9223372036854775808;
    $a = -$a;
    {
	no warnings 'imprecision';
	$c = --$a;
    }
    print "not "
	unless $a == -9223372036854775809 && $c == $a;
    print "ok 33\n";
    
    $a = 9223372036854775808;
    $a = -$a;
    $c = $a - 1;
    print "not "
	unless $a == -9223372036854775808 && $c == -9223372036854775809;
    print "ok 34\n";

    $a = 9223372036854775808;
    $b = -$a;
    {
	no warnings 'imprecision';
	$c = $b--;
    }
    print "not "
	unless $b == -$a-1 && $c == -$a;
    print "ok 35\n";

    $a = 9223372036854775808;
    $b = -$a;
    {
	no warnings 'imprecision';
	$c = --$b;
    }
    print "not "
	unless $b == -$a-1 && $c == $b;
    print "ok 36\n";

    $a = 9223372036854775808;
    $b = -$a;
    $b = $b - 1;
    print "not "
	unless $b == -(++$a);
    print "ok 37\n";

} else {
    # Unicos has imprecise doubles (14 decimal digits or so),
    # especially if operating near the UV/IV limits the low-order bits
    # become mangled even by simple arithmetic operations.
    for (23..37) {
	print "ok $_ # skipped: too imprecise numbers\n";
    }
}


$x = '';
print "not " unless (vec($x, 1, 64) = $q) == $q;
print "ok 38\n";

print "not " unless vec($x, 1, 64) == $q && vec($x, 1, 64) > $f;
print "ok 39\n";

print "not " unless vec($x, 0, 64) == 0 && vec($x, 2, 64) == 0;
print "ok 40\n";


print "not " unless ~0 == 0xffffffffffffffff;
print "ok 41\n";

print "not " unless (0xffffffff<<32) == 0xffffffff00000000;
print "ok 42\n";

print "not " unless ((0xffffffff)<<32)>>32 == 0xffffffff;
print "ok 43\n";

print "not " unless 1<<63 == 0x8000000000000000;
print "ok 44\n";

print "not " unless (sprintf "%#Vx", 1<<63) eq '0x8000000000000000';
print "ok 45\n";

print "not " unless (0x8000000000000000 | 1) == 0x8000000000000001;
print "ok 46\n";

print "not "
    unless (0xf000000000000000 & 0x8000000000000000) == 0x8000000000000000;
print "ok 47\n";

print "not "
    unless (0xf000000000000000 ^ 0xfffffffffffffff0) == 0x0ffffffffffffff0;
print "ok 48\n";


print "not "
    unless (sprintf "%b", ~0)   eq
           '1111111111111111111111111111111111111111111111111111111111111111';
print "ok 49\n";

print "not "
    unless (sprintf "%64b", ~0) eq
           '1111111111111111111111111111111111111111111111111111111111111111';
print "ok 50\n";

print "not " unless (sprintf "%d", ~0>>1) eq '9223372036854775807';
print "ok 51\n";

print "not " unless (sprintf "%u", ~0)    eq '18446744073709551615';
print "ok 52\n";

# If the 53..55 fail you have problems in the parser's string->int conversion,
# see toke.c:scan_num().

$q = -9223372036854775808;
print "# $q ne\n# -9223372036854775808\nnot " unless "$q" eq "-9223372036854775808";
print "ok 53\n";

$q =  9223372036854775807;
print "# $q ne\n# 9223372036854775807\nnot " unless "$q" eq "9223372036854775807";
print "ok 54\n";

$q = 18446744073709551615;
print "# $q ne\n# 18446744073709551615\nnot " unless "$q" eq "18446744073709551615";
print "ok 55\n";

# Test that sv_2nv then sv_2iv is the same as sv_2iv direct
# fails if whatever Atol is defined as can't actually cope with >32 bits.
my $num = 4294967297;
my $string = "4294967297";
{
  use integer;
  $num += 0;
  $string += 0;
}
if ($num eq $string) {
  print "ok 56\n";
} else {
  print "not ok 56 # \"$num\" ne \"$string\"\n";
}

# Test that sv_2nv then sv_2uv is the same as sv_2uv direct
$num = 4294967297;
$string = "4294967297";
$num &= 0;
$string &= 0;
if ($num eq $string) {
  print "ok 57\n";
} else {
  print "not ok 57 # \"$num\" ne \"$string\"\n";
}

$q = "18446744073709551616e0";
$q += 0;
print "# \"18446744073709551616e0\" += 0 gives $q\nnot " if "$q" eq "18446744073709551615";
print "ok 58\n";

# 0xFFFFFFFFFFFFFFFF ==  1 * 3 * 5 * 17 * 257 * 641 * 65537 * 6700417'
$q = 0xFFFFFFFFFFFFFFFF / 3;
if ($q == 0x5555555555555555 and ($q != 0x5555555555555556
                                  or !$maths_preserves_UVs)) {
  print "ok 59\n";
} else {
  print "not ok 59 # 0xFFFFFFFFFFFFFFFF / 3 = $q\n";
  print "# Should not be floating point\n" if $q =~ tr/e.//;
}

$q = 0xFFFFFFFFFFFFFFFF % 0x5555555555555555;
if ($q == 0) {
  print "ok 60\n";
} else {
  print "not ok 60 # 0xFFFFFFFFFFFFFFFF % 0x5555555555555555 => $q\n";
}

$q = 0xFFFFFFFFFFFFFFFF % 0xFFFFFFFFFFFFFFF0;
if ($q == 0xF) {
  print "ok 61\n";
} else {
  print "not ok 61 # 0xFFFFFFFFFFFFFFFF % 0xFFFFFFFFFFFFFFF0 => $q\n";
}

$q = 0x8000000000000000 % 9223372036854775807;
if ($q == 1) {
  print "ok 62\n";
} else {
  print "not ok 62 # 0x8000000000000000 % 9223372036854775807 => $q\n";
}

$q = 0x8000000000000000 % -9223372036854775807;
if ($q == -9223372036854775806) {
  print "ok 63\n";
} else {
  print "not ok 63 # 0x8000000000000000 % -9223372036854775807 => $q\n";
}

{
  use integer;
  $q = hex "0x123456789abcdef0";
  if ($q == 0x123456789abcdef0 and $q != 0x123456789abcdef1) {
    print "ok 64\n";
  } else {
    printf "not ok 64 # hex \"0x123456789abcdef0\" = $q (%X)\n", $q;
    print "# Should not be floating point\n" if $q =~ tr/e.//;
  }

  $q = oct "0x123456789abcdef0";
  if ($q == 0x123456789abcdef0 and $q != 0x123456789abcdef1) {
    print "ok 65\n";
  } else {
    printf "not ok 65 # oct \"0x123456789abcdef0\" = $q (%X)\n", $q;
    print "# Should not be floating point\n" if $q =~ tr/e.//;
  }

  $q = oct "765432176543217654321";
  if ($q == 0765432176543217654321 and $q != 0765432176543217654322) {
    print "ok 66\n";
  } else {
    printf "not ok 66 # oct \"765432176543217654321\" = $q (%o)\n", $q;
    print "# Should not be floating point\n" if $q =~ tr/e.//;
  }

  $q = oct "0b0101010101010101010101010101010101010101010101010101010101010101";
  if ($q == 0x5555555555555555 and $q != 0x5555555555555556) {
    print "ok 67\n";
  } else {
    printf "not ok 67 # oct \"0b0101010101010101010101010101010101010101010101010101010101010101\" = $q (%b)\n", $q;
    print "# Should not be floating point\n" if $q =~ tr/e.//;
  }
}

# eof
