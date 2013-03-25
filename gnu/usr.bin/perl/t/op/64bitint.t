#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
    eval { my $q = pack "q", 0 };
    skip_all('no 64-bit types') if $@;
}

# This could use many more tests.

# so that using > 0xfffffff constants and
# 32+ bit integers don't cause noise
use warnings;
no warnings qw(overflow portable);
use Config;

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
cmp_ok($x, '==', $q);
cmp_ok($x, '>', $f);


$x = sprintf("%lld", 12345678901);
is($x, $q);
cmp_ok($x, '>', $f);

$x = sprintf("%lld", $q);
cmp_ok($x, '==', $q);
is($x, $q);
cmp_ok($x, '>', $f);

$x = sprintf("%Ld", $q);
cmp_ok($x, '==', $q);
is($x, $q);
cmp_ok($x, '>', $f);

$x = sprintf("%qd", $q);
cmp_ok($x, '==', $q);
is($x, $q);
cmp_ok($x, '>', $f);


$x = sprintf("%llx", $q);
cmp_ok(hex $x, '==', 0x2dfdc1c35);
cmp_ok(hex $x, '>', $f);

$x = sprintf("%Lx", $q);
cmp_ok(hex $x, '==', 0x2dfdc1c35);
cmp_ok(hex $x, '>', $f);

$x = sprintf("%qx", $q);
cmp_ok(hex $x, '==', 0x2dfdc1c35);
cmp_ok(hex $x, '>', $f);

$x = sprintf("%llo", $q);
cmp_ok(oct "0$x", '==', 0133767016065);
cmp_ok(oct $x, '>', $f);

$x = sprintf("%Lo", $q);
cmp_ok(oct "0$x", '==', 0133767016065);
cmp_ok(oct $x, '>', $f);

$x = sprintf("%qo", $q);
cmp_ok(oct "0$x", '==', 0133767016065);
cmp_ok(oct $x, '>', $f);

$x = sprintf("%llb", $q);
cmp_ok(oct "0b$x", '==', 0b1011011111110111000001110000110101);
cmp_ok(oct "0b$x", '>', $f);

$x = sprintf("%Lb", $q);
cmp_ok(oct "0b$x", '==', 0b1011011111110111000001110000110101);
cmp_ok(oct "0b$x", '>', $f);

$x = sprintf("%qb", $q);
cmp_ok(oct "0b$x", '==', 0b1011011111110111000001110000110101);
cmp_ok(oct "0b$x", '>', $f);


$x = sprintf("%llu", $q);
is($x, $q);
cmp_ok($x, '>', $f);

$x = sprintf("%Lu", $q);
cmp_ok($x, '==', $q);
is($x, $q);
cmp_ok($x, '>', $f);

$x = sprintf("%qu", $q);
cmp_ok($x, '==', $q);
is($x, $q);
cmp_ok($x, '>', $f);


$x = sprintf("%D", $q);
cmp_ok($x, '==', $q);
is($x, $q);
cmp_ok($x, '>', $f);

$x = sprintf("%U", $q);
cmp_ok($x, '==', $q);
is($x, $q);
cmp_ok($x, '>', $f);

$x = sprintf("%O", $q);
cmp_ok(oct $x, '==', $q);
cmp_ok(oct $x, '>', $f);


$x = $q + $r;
cmp_ok($x, '==', 35802467913);
cmp_ok($x, '>', $f);

$x = $q - $r;
cmp_ok($x, '==', -11111110111);
cmp_ok(-$x, '>', $f);

SKIP: {
    # Unicos has imprecise doubles (14 decimal digits or so),
    # especially if operating near the UV/IV limits the low-order bits
    # become mangled even by simple arithmetic operations.
    skip('too imprecise numbers on unicos') if $^O eq 'unicos';

    $x = $q * 1234567;
    cmp_ok($x, '==', 15241567763770867);
    cmp_ok($x, '>', $f);

    $x /= 1234567;
    cmp_ok($x, '==', $q);
    cmp_ok($x, '>', $f);

    $x = 98765432109 % 12345678901;
    cmp_ok($x, '==', 901);

    # The following 12 tests adapted from op/inc.

    $a = 9223372036854775807;
    $c = $a++;
    cmp_ok($a, '==', 9223372036854775808);

    $a = 9223372036854775807;
    $c = ++$a;
    cmp_ok($a, '==', 9223372036854775808);
    cmp_ok($c, '==', $a);

    $a = 9223372036854775807;
    $c = $a + 1;
    cmp_ok($a, '==', 9223372036854775807);
    cmp_ok($c, '==', 9223372036854775808);

    $a = -9223372036854775808;
    {
	no warnings 'imprecision';
	$c = $a--;
    }
    cmp_ok($a, '==', -9223372036854775809);
    cmp_ok($c, '==', -9223372036854775808);

    $a = -9223372036854775808;
    {
	no warnings 'imprecision';
	$c = --$a;
    }
    cmp_ok($a, '==', -9223372036854775809);
    cmp_ok($c, '==', $a);

    $a = -9223372036854775808;
    $c = $a - 1;
    cmp_ok($a, '==', -9223372036854775808);
    cmp_ok($c, '==', -9223372036854775809);

    $a = 9223372036854775808;
    $a = -$a;
    {
	no warnings 'imprecision';
	$c = $a--;
    }
    cmp_ok($a, '==', -9223372036854775809);
    cmp_ok($c, '==', -9223372036854775808);

    $a = 9223372036854775808;
    $a = -$a;
    {
	no warnings 'imprecision';
	$c = --$a;
    }
    cmp_ok($a, '==', -9223372036854775809);
    cmp_ok($c, '==', $a);

    $a = 9223372036854775808;
    $a = -$a;
    $c = $a - 1;
    cmp_ok($a, '==', -9223372036854775808);
    cmp_ok($c, '==', -9223372036854775809);

    $a = 9223372036854775808;
    $b = -$a;
    {
	no warnings 'imprecision';
	$c = $b--;
    }
    cmp_ok($b, '==', -$a-1);
    cmp_ok($c, '==', -$a);

    $a = 9223372036854775808;
    $b = -$a;
    {
	no warnings 'imprecision';
	$c = --$b;
    }
    cmp_ok($b, '==', -$a-1);
    cmp_ok($c, '==', $b);

    $a = 9223372036854775808;
    $b = -$a;
    $b = $b - 1;
    cmp_ok($b, '==', -(++$a));
}


$x = '';
cmp_ok((vec($x, 1, 64) = $q), '==', $q);

cmp_ok(vec($x, 1, 64), '==', $q);
cmp_ok(vec($x, 1, 64), '>', $f);

cmp_ok(vec($x, 0, 64), '==', 0);
cmp_ok(vec($x, 2, 64), '==', 0);

cmp_ok(~0, '==', 0xffffffffffffffff);

cmp_ok((0xffffffff<<32), '==', 0xffffffff00000000);

cmp_ok(((0xffffffff)<<32)>>32, '==', 0xffffffff);

cmp_ok(1<<63, '==', 0x8000000000000000);

is((sprintf "%#Vx", 1<<63), '0x8000000000000000');

cmp_ok((0x8000000000000000 | 1), '==', 0x8000000000000001);

cmp_ok((0xf000000000000000 & 0x8000000000000000), '==', 0x8000000000000000);
cmp_ok((0xf000000000000000 ^ 0xfffffffffffffff0), '==', 0x0ffffffffffffff0);


is((sprintf "%b", ~0),
   '1111111111111111111111111111111111111111111111111111111111111111');


is((sprintf "%64b", ~0),
   '1111111111111111111111111111111111111111111111111111111111111111');

is((sprintf "%d", ~0>>1),'9223372036854775807');
is((sprintf "%u", ~0),'18446744073709551615');

# If the 53..55 fail you have problems in the parser's string->int conversion,
# see toke.c:scan_num().

$q = -9223372036854775808;
is("$q","-9223372036854775808");

$q =  9223372036854775807;
is("$q","9223372036854775807");

$q = 18446744073709551615;
is("$q","18446744073709551615");

# Test that sv_2nv then sv_2iv is the same as sv_2iv direct
# fails if whatever Atol is defined as can't actually cope with >32 bits.
my $num = 4294967297;
my $string = "4294967297";
{
  use integer;
  $num += 0;
  $string += 0;
}
is($num, $string);

# Test that sv_2nv then sv_2uv is the same as sv_2uv direct
$num = 4294967297;
$string = "4294967297";
$num &= 0;
$string &= 0;
is($num, $string);

$q = "18446744073709551616e0";
$q += 0;
isnt($q, "18446744073709551615");

# 0xFFFFFFFFFFFFFFFF ==  1 * 3 * 5 * 17 * 257 * 641 * 65537 * 6700417'
$q = 0xFFFFFFFFFFFFFFFF / 3;
cmp_ok($q, '==', 0x5555555555555555);
SKIP: {
    skip("Maths does not preserve UVs", 2) unless $maths_preserves_UVs;
    cmp_ok($q, '!=', 0x5555555555555556);
    skip("All UV division is precise as NVs, so is done as NVs", 1)
	if $Config{d_nv_preserves_uv};
    unlike($q, qr/[e.]/);
}

$q = 0xFFFFFFFFFFFFFFFF % 0x5555555555555555;
cmp_ok($q, '==', 0);

$q = 0xFFFFFFFFFFFFFFFF % 0xFFFFFFFFFFFFFFF0;
cmp_ok($q, '==', 0xF);

$q = 0x8000000000000000 % 9223372036854775807;
cmp_ok($q, '==', 1);

$q = 0x8000000000000000 % -9223372036854775807;
cmp_ok($q, '==', -9223372036854775806);

{
    use integer;
    $q = hex "0x123456789abcdef0";
    cmp_ok($q, '==', 0x123456789abcdef0);
    cmp_ok($q, '!=', 0x123456789abcdef1);
    unlike($q, qr/[e.]/, 'Should not be floating point');

    $q = oct "0x123456789abcdef0";
    cmp_ok($q, '==', 0x123456789abcdef0);
    cmp_ok($q, '!=', 0x123456789abcdef1);
    unlike($q, qr/[e.]/, 'Should not be floating point');

    $q = oct "765432176543217654321";
    cmp_ok($q, '==', 0765432176543217654321);
    cmp_ok($q, '!=', 0765432176543217654322);
    unlike($q, qr/[e.]/, 'Should not be floating point');

    $q = oct "0b0101010101010101010101010101010101010101010101010101010101010101";
    cmp_ok($q, '==', 0x5555555555555555);
    cmp_ok($q, '!=', 0x5555555555555556);
    unlike($q, qr/[e.]/, 'Should not be floating point');
}

done_testing();
