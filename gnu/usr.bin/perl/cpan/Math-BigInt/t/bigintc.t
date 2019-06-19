#!perl

use strict;
use warnings;

use Test::More tests => 379;

use Math::BigInt::Calc;

my ($BASE_LEN, undef, $AND_BITS, $XOR_BITS, $OR_BITS,
    $BASE_LEN_SMALL, $MAX_VAL)
  = Math::BigInt::Calc->_base_len();

print "# BASE_LEN = $BASE_LEN\n";
print "# MAX_VAL  = $MAX_VAL\n";
print "# AND_BITS = $AND_BITS\n";
print "# XOR_BITS = $XOR_BITS\n";
print "# IOR_BITS = $OR_BITS\n";

# testing of Math::BigInt::Calc

my $CALC = 'Math::BigInt::Calc';		# pass classname to sub's

# _new and _str

my $x = $CALC->_new("123");
my $y = $CALC->_new("321");
is(ref($x), "Math::BigInt::Calc", q|ref($x) is an Math::BigInt::Calc|);
is($CALC->_str($x), 123,     qq|$CALC->_str(\$x) = 123|);
is($CALC->_str($y), 321,     qq|$CALC->_str(\$y) = 321|);

###############################################################################
# _add, _sub, _mul, _div

is($CALC->_str($CALC->_add($x, $y)), 444,
   qq|$CALC->_str($CALC->_add(\$x, \$y)) = 444|);
is($CALC->_str($CALC->_sub($x, $y)), 123,
   qq|$CALC->_str($CALC->_sub(\$x, \$y)) = 123|);
is($CALC->_str($CALC->_mul($x, $y)), 39483,
   qq|$CALC->_str($CALC->_mul(\$x, \$y)) = 39483|);
is($CALC->_str($CALC->_div($x, $y)), 123,
   qq|$CALC->_str($CALC->_div(\$x, \$y)) = 123|);

###############################################################################
# check that mul/div doesn't change $y
# and returns the same reference, not something new

is($CALC->_str($CALC->_mul($x, $y)), 39483,
   qq|$CALC->_str($CALC->_mul(\$x, \$y)) = 39483|);
is($CALC->_str($x), 39483,
   qq|$CALC->_str(\$x) = 39483|);
is($CALC->_str($y), 321,
   qq|$CALC->_str(\$y) = 321|);

is($CALC->_str($CALC->_div($x, $y)), 123,
   qq|$CALC->_str($CALC->_div(\$x, \$y)) = 123|);
is($CALC->_str($x), 123,
   qq|$CALC->_str(\$x) = 123|);
is($CALC->_str($y), 321,
   qq|$CALC->_str(\$y) = 321|);

$x = $CALC->_new("39483");
my ($x1, $r1) = $CALC->_div($x, $y);
is("$x1", "$x", q|"$x1" = "$x"|);
$CALC->_inc($x1);
is("$x1", "$x", q|"$x1" = "$x"|);
is($CALC->_str($r1), "0", qq|$CALC->_str(\$r1) = "0"|);

$x = $CALC->_new("39483");	# reset

###############################################################################

my $z = $CALC->_new("2");
is($CALC->_str($CALC->_add($x, $z)), 39485,
   qq|$CALC->_str($CALC->_add(\$x, \$z)) = 39485|);
my ($re, $rr) = $CALC->_div($x, $y);

is($CALC->_str($re), 123, qq|$CALC->_str(\$re) = 123|);
is($CALC->_str($rr), 2,   qq|$CALC->_str(\$rr) = 2|);

# is_zero, _is_one, _one, _zero

is($CALC->_is_zero($x) || 0, 0, qq/$CALC->_is_zero(\$x) || 0 = 0/);
is($CALC->_is_one($x)  || 0, 0, qq/$CALC->_is_one(\$x)  || 0 = 0/);

is($CALC->_str($CALC->_zero()), "0", qq|$CALC->_str($CALC->_zero()) = "0"|);
is($CALC->_str($CALC->_one()),  "1", qq|$CALC->_str($CALC->_one())  = "1"|);

# _two() and _ten()

is($CALC->_str($CALC->_two()),    "2",  qq|$CALC->_str($CALC->_two()) = "2"|);
is($CALC->_str($CALC->_ten()),    "10", qq|$CALC->_str($CALC->_ten()) = "10"|);
is($CALC->_is_ten($CALC->_two()), 0,    qq|$CALC->_is_ten($CALC->_two()) = 0|);
is($CALC->_is_two($CALC->_two()), 1,    qq|$CALC->_is_two($CALC->_two()) = 1|);
is($CALC->_is_ten($CALC->_ten()), 1,    qq|$CALC->_is_ten($CALC->_ten()) = 1|);
is($CALC->_is_two($CALC->_ten()), 0,    qq|$CALC->_is_two($CALC->_ten()) = 0|);

is($CALC->_is_one($CALC->_one()), 1,    qq|$CALC->_is_one($CALC->_one()) = 1|);
is($CALC->_is_one($CALC->_two()), 0,    qq|$CALC->_is_one($CALC->_two()) = 0|);
is($CALC->_is_one($CALC->_ten()), 0,    qq|$CALC->_is_one($CALC->_ten()) = 0|);

is($CALC->_is_one($CALC->_zero()) || 0, 0,
   qq/$CALC->_is_one($CALC->_zero()) || 0 = 0/);

is($CALC->_is_zero($CALC->_zero()), 1,
   qq|$CALC->_is_zero($CALC->_zero()) = 1|);

is($CALC->_is_zero($CALC->_one()) || 0, 0,
   qq/$CALC->_is_zero($CALC->_one()) || 0 = 0/);

# is_odd, is_even

is($CALC->_is_odd($CALC->_one()), 1,
   qq/$CALC->_is_odd($CALC->_one()) = 1/);
is($CALC->_is_odd($CALC->_zero()) || 0, 0,
   qq/$CALC->_is_odd($CALC->_zero()) || 0 = 0/);
is($CALC->_is_even($CALC->_one()) || 0, 0,
   qq/$CALC->_is_even($CALC->_one()) || 0 = 0/);
is($CALC->_is_even($CALC->_zero()), 1,
   qq/$CALC->_is_even($CALC->_zero()) = 1/);

# _len

for my $method (qw/_alen _len/) {
    $x = $CALC->_new("1");
    is($CALC->$method($x), 1, qq|$CALC->$method(\$x) = 1|);
    $x = $CALC->_new("12");
    is($CALC->$method($x), 2, qq|$CALC->$method(\$x) = 2|);
    $x = $CALC->_new("123");
    is($CALC->$method($x), 3, qq|$CALC->$method(\$x) = 3|);
    $x = $CALC->_new("1234");
    is($CALC->$method($x), 4, qq|$CALC->$method(\$x) = 4|);
    $x = $CALC->_new("12345");
    is($CALC->$method($x), 5, qq|$CALC->$method(\$x) = 5|);
    $x = $CALC->_new("123456");
    is($CALC->$method($x), 6, qq|$CALC->$method(\$x) = 6|);
    $x = $CALC->_new("1234567");
    is($CALC->$method($x), 7, qq|$CALC->$method(\$x) = 7|);
    $x = $CALC->_new("12345678");
    is($CALC->$method($x), 8, qq|$CALC->$method(\$x) = 8|);
    $x = $CALC->_new("123456789");
    is($CALC->$method($x), 9, qq|$CALC->$method(\$x) = 9|);

    $x = $CALC->_new("8");
    is($CALC->$method($x), 1, qq|$CALC->$method(\$x) = 1|);
    $x = $CALC->_new("21");
    is($CALC->$method($x), 2, qq|$CALC->$method(\$x) = 2|);
    $x = $CALC->_new("321");
    is($CALC->$method($x), 3, qq|$CALC->$method(\$x) = 3|);
    $x = $CALC->_new("4321");
    is($CALC->$method($x), 4, qq|$CALC->$method(\$x) = 4|);
    $x = $CALC->_new("54321");
    is($CALC->$method($x), 5, qq|$CALC->$method(\$x) = 5|);
    $x = $CALC->_new("654321");
    is($CALC->$method($x), 6, qq|$CALC->$method(\$x) = 6|);
    $x = $CALC->_new("7654321");
    is($CALC->$method($x), 7, qq|$CALC->$method(\$x) = 7|);
    $x = $CALC->_new("87654321");
    is($CALC->$method($x), 8, qq|$CALC->$method(\$x) = 8|);
    $x = $CALC->_new("987654321");
    is($CALC->$method($x), 9, qq|$CALC->$method(\$x) = 9|);

    $x = $CALC->_new("0");
    is($CALC->$method($x), 1, qq|$CALC->$method(\$x) = 1|);
    $x = $CALC->_new("20");
    is($CALC->$method($x), 2, qq|$CALC->$method(\$x) = 2|);
    $x = $CALC->_new("320");
    is($CALC->$method($x), 3, qq|$CALC->$method(\$x) = 3|);
    $x = $CALC->_new("4320");
    is($CALC->$method($x), 4, qq|$CALC->$method(\$x) = 4|);
    $x = $CALC->_new("54320");
    is($CALC->$method($x), 5, qq|$CALC->$method(\$x) = 5|);
    $x = $CALC->_new("654320");
    is($CALC->$method($x), 6, qq|$CALC->$method(\$x) = 6|);
    $x = $CALC->_new("7654320");
    is($CALC->$method($x), 7, qq|$CALC->$method(\$x) = 7|);
    $x = $CALC->_new("87654320");
    is($CALC->$method($x), 8, qq|$CALC->$method(\$x) = 8|);
    $x = $CALC->_new("987654320");
    is($CALC->$method($x), 9, qq|$CALC->$method(\$x) = 9|);

    for (my $i = 1; $i < 9; $i++) {
        my $a = "$i" . '0' x ($i - 1);
        $x = $CALC->_new($a);
        is($CALC->_len($x), $i, qq|$CALC->_len(\$x) = $i|);
    }
}

# _digit

$x = $CALC->_new("123456789");
is($CALC->_digit($x, 0),   9, qq|$CALC->_digit(\$x, 0) = 9|);
is($CALC->_digit($x, 1),   8, qq|$CALC->_digit(\$x, 1) = 8|);
is($CALC->_digit($x, 2),   7, qq|$CALC->_digit(\$x, 2) = 7|);
is($CALC->_digit($x, 8),   1, qq|$CALC->_digit(\$x, 8) = 1|);
is($CALC->_digit($x, 9),   0, qq|$CALC->_digit(\$x, 9) = 0|);
is($CALC->_digit($x, -1),  1, qq|$CALC->_digit(\$x, -1) = 1|);
is($CALC->_digit($x, -2),  2, qq|$CALC->_digit(\$x, -2) = 2|);
is($CALC->_digit($x, -3),  3, qq|$CALC->_digit(\$x, -3) = 3|);
is($CALC->_digit($x, -9),  9, qq|$CALC->_digit(\$x, -9) = 9|);
is($CALC->_digit($x, -10), 0, qq|$CALC->_digit(\$x, -10) = 0|);

# _copy

foreach (qw/ 1 12 123 1234 12345 123456 1234567 12345678 123456789/) {
    $x = $CALC->_new("$_");
    is($CALC->_str($CALC->_copy($x)), "$_",
       qq|$CALC->_str($CALC->_copy(\$x)) = "$_"|);
    is($CALC->_str($x), "$_",           # did _copy destroy original x?
       qq|$CALC->_str(\$x) = "$_"|);
}

# _zeros

$x = $CALC->_new("1256000000");
is($CALC->_zeros($x), 6, qq|$CALC->_zeros(\$x) = 6|);

$x = $CALC->_new("152");
is($CALC->_zeros($x), 0, qq|$CALC->_zeros(\$x) = 0|);

$x = $CALC->_new("123000");
is($CALC->_zeros($x), 3, qq|$CALC->_zeros(\$x) = 3|);

$x = $CALC->_new("0");
is($CALC->_zeros($x), 0, qq|$CALC->_zeros(\$x) = 0|);

# _lsft, _rsft

$x = $CALC->_new("10");
$y = $CALC->_new("3");
is($CALC->_str($CALC->_lsft($x, $y, 10)), 10000,
   qq|$CALC->_str($CALC->_lsft(\$x, \$y, 10)) = 10000|);

$x = $CALC->_new("20");
$y = $CALC->_new("3");
is($CALC->_str($CALC->_lsft($x, $y, 10)), 20000,
   qq|$CALC->_str($CALC->_lsft(\$x, \$y, 10)) = 20000|);

$x = $CALC->_new("128");
$y = $CALC->_new("4");
is($CALC->_str($CALC->_lsft($x, $y, 2)), 128 << 4,
   qq|$CALC->_str($CALC->_lsft(\$x, \$y, 2)) = 128 << 4|);

$x = $CALC->_new("1000");
$y = $CALC->_new("3");
is($CALC->_str($CALC->_rsft($x, $y, 10)), 1,
   qq|$CALC->_str($CALC->_rsft(\$x, \$y, 10)) = 1|);

$x = $CALC->_new("20000");
$y = $CALC->_new("3");
is($CALC->_str($CALC->_rsft($x, $y, 10)), 20,
   qq|$CALC->_str($CALC->_rsft(\$x, \$y, 10)) = 20|);

$x = $CALC->_new("256");
$y = $CALC->_new("4");
is($CALC->_str($CALC->_rsft($x, $y, 2)), 256 >> 4,
   qq|$CALC->_str($CALC->_rsft(\$x, \$y, 2)) = 256 >> 4|);

$x = $CALC->_new("6411906467305339182857313397200584952398");
$y = $CALC->_new("45");
is($CALC->_str($CALC->_rsft($x, $y, 10)), 0,
   qq|$CALC->_str($CALC->_rsft(\$x, \$y, 10)) = 0|);

# _acmp

$x = $CALC->_new("123456789");
$y = $CALC->_new("987654321");
is($CALC->_acmp($x, $y), -1, qq|$CALC->_acmp(\$x, \$y) = -1|);
is($CALC->_acmp($y, $x), 1,  qq|$CALC->_acmp(\$y, \$x) = 1|);
is($CALC->_acmp($x, $x), 0,  qq|$CALC->_acmp(\$x, \$x) = 0|);
is($CALC->_acmp($y, $y), 0,  qq|$CALC->_acmp(\$y, \$y) = 0|);
$x = $CALC->_new("12");
$y = $CALC->_new("12");
is($CALC->_acmp($x, $y), 0,  qq|$CALC->_acmp(\$x, \$y) = 0|);
$x = $CALC->_new("21");
is($CALC->_acmp($x, $y), 1,  qq|$CALC->_acmp(\$x, \$y) = 1|);
is($CALC->_acmp($y, $x), -1, qq|$CALC->_acmp(\$y, \$x) = -1|);
$x = $CALC->_new("123456789");
$y = $CALC->_new("1987654321");
is($CALC->_acmp($x, $y), -1, qq|$CALC->_acmp(\$x, \$y) = -1|);
is($CALC->_acmp($y, $x), +1, qq|$CALC->_acmp(\$y, \$x) = +1|);

$x = $CALC->_new("1234567890123456789");
$y = $CALC->_new("987654321012345678");
is($CALC->_acmp($x, $y), 1,  qq|$CALC->_acmp(\$x, \$y) = 1|);
is($CALC->_acmp($y, $x), -1, qq|$CALC->_acmp(\$y, \$x) = -1|);
is($CALC->_acmp($x, $x), 0,  qq|$CALC->_acmp(\$x, \$x) = 0|);
is($CALC->_acmp($y, $y), 0,  qq|$CALC->_acmp(\$y, \$y) = 0|);

$x = $CALC->_new("1234");
$y = $CALC->_new("987654321012345678");
is($CALC->_acmp($x, $y), -1, qq|$CALC->_acmp(\$x, \$y) = -1|);
is($CALC->_acmp($y, $x), 1,  qq|$CALC->_acmp(\$y, \$x) = 1|);
is($CALC->_acmp($x, $x), 0,  qq|$CALC->_acmp(\$x, \$x) = 0|);
is($CALC->_acmp($y, $y), 0,  qq|$CALC->_acmp(\$y, \$y) = 0|);

# _modinv

$x = $CALC->_new("8");
$y = $CALC->_new("5033");
my ($xmod, $sign) = $CALC->_modinv($x, $y);
is($CALC->_str($xmod), "629", 	        # -629 % 5033 == 4404
   qq|$CALC->_str(\$xmod) = "629"|);
is($sign, "-", q|$sign = "-"|);

# _div

$x = $CALC->_new("3333");
$y = $CALC->_new("1111");
is($CALC->_str(scalar($CALC->_div($x, $y))), 3,
   qq|$CALC->_str(scalar($CALC->_div(\$x, \$y))) = 3|);

$x = $CALC->_new("33333");
$y = $CALC->_new("1111");
($x, $y) = $CALC->_div($x, $y);
is($CALC->_str($x), 30, qq|$CALC->_str(\$x) = 30|);
is($CALC->_str($y),  3, qq|$CALC->_str(\$y) = 3|);

$x = $CALC->_new("123");
$y = $CALC->_new("1111");
($x, $y) = $CALC->_div($x, $y);
is($CALC->_str($x), 0,   qq|$CALC->_str(\$x) = 0|);
is($CALC->_str($y), 123, qq|$CALC->_str(\$y) = 123|);

# _num

foreach (qw/1 12 123 1234 12345 1234567 12345678 123456789 1234567890/) {

    $x = $CALC->_new("$_");
    is(ref($x), "Math::BigInt::Calc",
       q|ref($x) = "Math::BigInt::Calc"|);
    is($CALC->_str($x), "$_", qq|$CALC->_str(\$x) = "$_"|);

    $x = $CALC->_num($x);
    is(ref($x), "", q|ref($x) = ""|);
    is($x,      $_, qq|\$x = $_|);
}

# _sqrt

$x = $CALC->_new("144");
is($CALC->_str($CALC->_sqrt($x)), "12",
   qq|$CALC->_str($CALC->_sqrt(\$x)) = "12"|);
$x = $CALC->_new("144000000000000");
is($CALC->_str($CALC->_sqrt($x)), "12000000",
   qq|$CALC->_str($CALC->_sqrt(\$x)) = "12000000"|);

# _root

$x = $CALC->_new("81");
my $n = $CALC->_new("3"); 	# 4*4*4 = 64, 5*5*5 = 125
is($CALC->_str($CALC->_root($x, $n)), "4",
   qq|$CALC->_str($CALC->_root(\$x, \$n)) = "4"|); # 4.xx => 4.0

$x = $CALC->_new("81");
$n = $CALC->_new("4");          # 3*3*3*3 == 81
is($CALC->_str($CALC->_root($x, $n)), "3",
   qq|$CALC->_str($CALC->_root(\$x, \$n)) = "3"|);

# _pow (and _root)

$x = $CALC->_new("0");
$n = $CALC->_new("3");          # 0 ** y => 0
is($CALC->_str($CALC->_pow($x, $n)), 0,
   qq|$CALC->_str($CALC->_pow(\$x, \$n)) = 0|);

$x = $CALC->_new("3");
$n = $CALC->_new("0");          # x ** 0 => 1
is($CALC->_str($CALC->_pow($x, $n)), 1,
   qq|$CALC->_str($CALC->_pow(\$x, \$n)) = 1|);

$x = $CALC->_new("1");
$n = $CALC->_new("3");          # 1 ** y => 1
is($CALC->_str($CALC->_pow($x, $n)), 1,
   qq|$CALC->_str($CALC->_pow(\$x, \$n)) = 1|);

$x = $CALC->_new("5");
$n = $CALC->_new("1");          # x ** 1 => x
is($CALC->_str($CALC->_pow($x, $n)), 5,
   qq|$CALC->_str($CALC->_pow(\$x, \$n)) = 5|);

$x = $CALC->_new("81");
$n = $CALC->_new("3");          # 81 ** 3 == 531441
is($CALC->_str($CALC->_pow($x, $n)), 81 ** 3,
   qq|$CALC->_str($CALC->_pow(\$x, \$n)) = 81 ** 3|);

is($CALC->_str($CALC->_root($x, $n)), 81,
   qq|$CALC->_str($CALC->_root(\$x, \$n)) = 81|);

$x = $CALC->_new("81");
is($CALC->_str($CALC->_pow($x, $n)), 81 ** 3,
   qq|$CALC->_str($CALC->_pow(\$x, \$n)) = 81 ** 3|);
is($CALC->_str($CALC->_pow($x, $n)), "150094635296999121",      # 531441 ** 3
   qq|$CALC->_str($CALC->_pow(\$x, \$n)) = "150094635296999121"|);

is($CALC->_str($CALC->_root($x, $n)), "531441",
   qq|$CALC->_str($CALC->_root(\$x, \$n)) = "531441"|);
is($CALC->_str($CALC->_root($x, $n)), "81",
   qq|$CALC->_str($CALC->_root(\$x, \$n)) = "81"|);

$x = $CALC->_new("81");
$n = $CALC->_new("14");
is($CALC->_str($CALC->_pow($x, $n)), "523347633027360537213511521",
   qq|$CALC->_str($CALC->_pow(\$x, \$n)) = "523347633027360537213511521"|);
is($CALC->_str($CALC->_root($x, $n)), "81",
   qq|$CALC->_str($CALC->_root(\$x, \$n)) = "81"|);

$x = $CALC->_new("523347633027360537213511520");
is($CALC->_str($CALC->_root($x, $n)), "80",
   qq|$CALC->_str($CALC->_root(\$x, \$n)) = "80"|);

$x = $CALC->_new("523347633027360537213511522");
is($CALC->_str($CALC->_root($x, $n)), "81",
   qq|$CALC->_str($CALC->_root(\$x, \$n)) = "81"|);

my $res = [ qw/9 31 99 316 999 3162 9999 31622 99999/ ];

# 99 ** 2 = 9801, 999 ** 2 = 998001 etc

for my $i (2 .. 9) {
    $x = '9' x $i;
    $x = $CALC->_new($x);
    $n = $CALC->_new("2");
    my $rc = '9' x ($i-1). '8' . '0' x ($i - 1) . '1';
    print "# _pow( ", '9' x $i, ", 2) \n" unless
      is($CALC->_str($CALC->_pow($x, $n)), $rc,
         qq|$CALC->_str($CALC->_pow(\$x, \$n)) = $rc|);

  SKIP: {
        # If $i > $BASE_LEN, the test takes a really long time.
        skip "$i > $BASE_LEN", 2 unless $i <= $BASE_LEN;

        $x = '9' x $i;
        $x = $CALC->_new($x);
        $n = '9' x $i;
        $n = $CALC->_new($n);
        print "# _root( ", '9' x $i, ", ", 9 x $i, ") \n";
        print "# _root( ", '9' x $i, ", ", 9 x $i, ") \n"
          unless is($CALC->_str($CALC->_root($x, $n)), '1',
                    qq|$CALC->_str($CALC->_root(\$x, \$n)) = '1'|);

        $x = '9' x $i;
        $x = $CALC->_new($x);
        $n = $CALC->_new("2");
        print "# BASE_LEN $BASE_LEN _root( ", '9' x $i, ", ", 9 x $i, ") \n"
          unless is($CALC->_str($CALC->_root($x, $n)), $res->[$i-2],
                    qq|$CALC->_str($CALC->_root(\$x, \$n)) = $res->[$i-2]|);
    }
}

##############################################################################
# _fac

$x = $CALC->_new("0");
is($CALC->_str($CALC->_fac($x)), "1",
   qq|$CALC->_str($CALC->_fac(\$x)) = "1"|);

$x = $CALC->_new("1");
is($CALC->_str($CALC->_fac($x)), "1",
   qq|$CALC->_str($CALC->_fac(\$x)) = "1"|);

$x = $CALC->_new("2");
is($CALC->_str($CALC->_fac($x)), "2",
   qq|$CALC->_str($CALC->_fac(\$x)) = "2"|);

$x = $CALC->_new("3");
is($CALC->_str($CALC->_fac($x)), "6",
   qq|$CALC->_str($CALC->_fac(\$x)) = "6"|);

$x = $CALC->_new("4");
is($CALC->_str($CALC->_fac($x)), "24",
   qq|$CALC->_str($CALC->_fac(\$x)) = "24"|);

$x = $CALC->_new("5");
is($CALC->_str($CALC->_fac($x)), "120",
   qq|$CALC->_str($CALC->_fac(\$x)) = "120"|);

$x = $CALC->_new("10");
is($CALC->_str($CALC->_fac($x)), "3628800",
   qq|$CALC->_str($CALC->_fac(\$x)) = "3628800"|);

$x = $CALC->_new("11");
is($CALC->_str($CALC->_fac($x)), "39916800",
   qq|$CALC->_str($CALC->_fac(\$x)) = "39916800"|);

$x = $CALC->_new("12");
is($CALC->_str($CALC->_fac($x)), "479001600",
   qq|$CALC->_str($CALC->_fac(\$x)) = "479001600"|);

$x = $CALC->_new("13");
is($CALC->_str($CALC->_fac($x)), "6227020800",
   qq|$CALC->_str($CALC->_fac(\$x)) = "6227020800"|);

# test that _fac modifies $x in place for small arguments

$x = $CALC->_new("3");
$CALC->_fac($x);
is($CALC->_str($x), "6",
   qq|$CALC->_str(\$x) = "6"|);

$x = $CALC->_new("13");
$CALC->_fac($x);
is($CALC->_str($x), "6227020800",
   qq|$CALC->_str(\$x) = "6227020800"|);

##############################################################################
# _inc and _dec

for (qw/1 11 121 1231 12341 1234561 12345671 123456781 1234567891/) {
    $x = $CALC->_new("$_");
    $CALC->_inc($x);
    my $expected = substr($_, 0, length($_) - 1) . '2';
    is($CALC->_str($x), $expected, qq|$CALC->_str(\$x) = $expected|);
    $CALC->_dec($x);
    is($CALC->_str($x), $_, qq|$CALC->_str(\$x) = $_|);
}

for (qw/19 119 1219 12319 1234519 12345619 123456719 1234567819/) {
    $x = $CALC->_new("$_");
    $CALC->_inc($x);
    my $expected = substr($_, 0, length($_)-2) . '20';
    is($CALC->_str($x), $expected, qq|$CALC->_str(\$x) = $expected|);
    $CALC->_dec($x);
    is($CALC->_str($x), $_, qq|$CALC->_str(\$x) = $_|);
}

for (qw/999 9999 99999 9999999 99999999 999999999 9999999999 99999999999/) {
    $x = $CALC->_new("$_");
    $CALC->_inc($x);
    my $expected = '1' . '0' x (length($_));
    is($CALC->_str($x), $expected, qq|$CALC->_str(\$x) = $expected|);
    $CALC->_dec($x);
    is($CALC->_str($x), $_, qq|$CALC->_str(\$x) = $_|);
}

$x = $CALC->_new("1000");
$CALC->_inc($x);
is($CALC->_str($x), "1001", qq|$CALC->_str(\$x) = "1001"|);
$CALC->_dec($x);
is($CALC->_str($x), "1000", qq|$CALC->_str(\$x) = "1000"|);

my $BL;
{
    no strict 'refs';
    $BL = &{"$CALC"."::_base_len"}();
}

$x = '1' . '0' x $BL;
$z = '1' . '0' x ($BL - 1);
$z .= '1';
$x = $CALC->_new($x);
$CALC->_inc($x);
is($CALC->_str($x), $z, qq|$CALC->_str(\$x) = $z|);

$x = '1' . '0' x $BL;
$z = '9' x $BL;
$x = $CALC->_new($x);
$CALC->_dec($x);
is($CALC->_str($x), $z, qq|$CALC->_str(\$x) = $z|);

# should not happen:
# $x = $CALC->_new("-2");
# $y = $CALC->_new("4");
# is($CALC->_acmp($x, $y), -1, qq|$CALC->_acmp($x, $y) = -1|);

###############################################################################
# _mod

$x = $CALC->_new("1000");
$y = $CALC->_new("3");
is($CALC->_str(scalar($CALC->_mod($x, $y))), 1,
   qq|$CALC->_str(scalar($CALC->_mod(\$x, \$y))) = 1|);

$x = $CALC->_new("1000");
$y = $CALC->_new("2");
is($CALC->_str(scalar($CALC->_mod($x, $y))), 0,
   qq|$CALC->_str(scalar($CALC->_mod(\$x, \$y))) = 0|);

# _and, _or, _xor

$x = $CALC->_new("5");
$y = $CALC->_new("2");
is($CALC->_str(scalar($CALC->_xor($x, $y))), 7,
   qq|$CALC->_str(scalar($CALC->_xor(\$x, \$y))) = 7|);

$x = $CALC->_new("5");
$y = $CALC->_new("2");
is($CALC->_str(scalar($CALC->_or($x, $y))), 7,
   qq|$CALC->_str(scalar($CALC->_or(\$x, \$y))) = 7|);

$x = $CALC->_new("5");
$y = $CALC->_new("3");
is($CALC->_str(scalar($CALC->_and($x, $y))), 1,
   qq|$CALC->_str(scalar($CALC->_and(\$x, \$y))) = 1|);

# _from_hex, _from_bin, _from_oct

is($CALC->_str($CALC->_from_hex("0xFf")), 255,
   qq|$CALC->_str($CALC->_from_hex("0xFf")) = 255|);
is($CALC->_str($CALC->_from_bin("0b10101011")), 160+11,
   qq|$CALC->_str($CALC->_from_bin("0b10101011")) = 160+11|);
is($CALC->_str($CALC->_from_oct("0100")), 8*8,
   qq|$CALC->_str($CALC->_from_oct("0100")) = 8*8|);
is($CALC->_str($CALC->_from_oct("01000")), 8*8*8,
   qq|$CALC->_str($CALC->_from_oct("01000")) = 8*8*8|);
is($CALC->_str($CALC->_from_oct("010001")), 8*8*8*8+1,
   qq|$CALC->_str($CALC->_from_oct("010001")) = 8*8*8*8+1|);
is($CALC->_str($CALC->_from_oct("010007")), 8*8*8*8+7,
   qq|$CALC->_str($CALC->_from_oct("010007")) = 8*8*8*8+7|);

# _as_hex, _as_bin, as_oct

is($CALC->_str($CALC->_from_hex($CALC->_as_hex($CALC->_new("128")))), 128,
   qq|$CALC->_str($CALC->_from_hex($CALC->_as_hex(|
   . qq|$CALC->_new("128")))) = 128|);
is($CALC->_str($CALC->_from_bin($CALC->_as_bin($CALC->_new("128")))), 128,
   qq|$CALC->_str($CALC->_from_bin($CALC->_as_bin(|
   . qq|$CALC->_new("128")))) = 128|);
is($CALC->_str($CALC->_from_oct($CALC->_as_oct($CALC->_new("128")))), 128,
   qq|$CALC->_str($CALC->_from_oct($CALC->_as_oct(|
   . qq|$CALC->_new("128")))) = 128|);

is($CALC->_str($CALC->_from_oct($CALC->_as_oct($CALC->_new("123456")))),
   123456,
   qq|$CALC->_str($CALC->_from_oct($CALC->_as_oct|
   . qq|($CALC->_new("123456")))) = 123456|);
is($CALC->_str($CALC->_from_oct($CALC->_as_oct($CALC->_new("123456789")))),
   "123456789",
   qq|$CALC->_str($CALC->_from_oct($CALC->_as_oct(|
   . qq|$CALC->_new("123456789")))) = "123456789"|);
is($CALC->_str($CALC->_from_oct($CALC->_as_oct($CALC->_new("1234567890123")))),
   "1234567890123",
   qq|$CALC->_str($CALC->_from_oct($CALC->_as_oct(|
   . qq|$CALC->_new("1234567890123")))) = "1234567890123"|);

my $long = "123456789012345678901234567890";
is($CALC->_str($CALC->_from_hex($CALC->_as_hex($CALC->_new($long)))), $long,
   qq|$CALC->_str($CALC->_from_hex($CALC->_as_hex(|
   . qq|$CALC->_new("$long")))) = "$long"|);
is($CALC->_str($CALC->_from_bin($CALC->_as_bin($CALC->_new($long)))), $long,
   qq|$CALC->_str($CALC->_from_bin($CALC->_as_bin(|
   . qq|$CALC->_new("$long")))) = "$long"|);
is($CALC->_str($CALC->_from_oct($CALC->_as_oct($CALC->_new($long)))), $long,
   qq|$CALC->_str($CALC->_from_oct($CALC->_as_oct(|
   . qq|$CALC->_new("$long")))) = "$long"|);

is($CALC->_str($CALC->_from_hex($CALC->_as_hex($CALC->_new("0")))), 0,
   qq|$CALC->_str($CALC->_from_hex($CALC->_as_hex(|
   . qq|$CALC->_new("0")))) = 0|);
is($CALC->_str($CALC->_from_bin($CALC->_as_bin($CALC->_new("0")))), 0,
   qq|$CALC->_str($CALC->_from_bin($CALC->_as_bin(|
   . qq|$CALC->_new("0")))) = 0|);
is($CALC->_str($CALC->_from_oct($CALC->_as_oct($CALC->_new("0")))), 0,
   qq|$CALC->_str($CALC->_from_oct($CALC->_as_oct(|
   . qq|$CALC->_new("0")))) = 0|);

is($CALC->_as_hex($CALC->_new("0")), "0x0",
   qq|$CALC->_as_hex($CALC->_new("0")) = "0x0"|);
is($CALC->_as_bin($CALC->_new("0")), "0b0",
   qq|$CALC->_as_bin($CALC->_new("0")) = "0b0"|);
is($CALC->_as_oct($CALC->_new("0")), "00",
   qq|$CALC->_as_oct($CALC->_new("0")) = "00"|);

is($CALC->_as_hex($CALC->_new("12")), "0xc",
   qq|$CALC->_as_hex($CALC->_new("12")) = "0xc"|);
is($CALC->_as_bin($CALC->_new("12")), "0b1100",
   qq|$CALC->_as_bin($CALC->_new("12")) = "0b1100"|);
is($CALC->_as_oct($CALC->_new("64")), "0100",
   qq|$CALC->_as_oct($CALC->_new("64")) = "0100"|);

# _1ex

is($CALC->_str($CALC->_1ex(0)), "1",
   qq|$CALC->_str($CALC->_1ex(0)) = "1"|);
is($CALC->_str($CALC->_1ex(1)), "10",
   qq|$CALC->_str($CALC->_1ex(1)) = "10"|);
is($CALC->_str($CALC->_1ex(2)), "100",
   qq|$CALC->_str($CALC->_1ex(2)) = "100"|);
is($CALC->_str($CALC->_1ex(12)), "1000000000000",
   qq|$CALC->_str($CALC->_1ex(12)) = "1000000000000"|);
is($CALC->_str($CALC->_1ex(16)), "10000000000000000",
   qq|$CALC->_str($CALC->_1ex(16)) = "10000000000000000"|);

# _check

$x = $CALC->_new("123456789");
is($CALC->_check($x), 0,
   qq|$CALC->_check(\$x) = 0|);
is($CALC->_check(123), "123 is not a reference",
   qq|$CALC->_check(123) = "123 is not a reference"|);

###############################################################################
# __strip_zeros

{
    no strict 'refs';

    # correct empty arrays
    $x = &{$CALC."::__strip_zeros"}([]);
    is(@$x, 1, q|@$x = 1|);
    is($x->[0], 0, q|$x->[0] = 0|);

    # don't strip single elements
    $x = &{$CALC."::__strip_zeros"}([0]);
    is(@$x, 1, q|@$x = 1|);
    is($x->[0], 0, q|$x->[0] = 0|);
    $x = &{$CALC."::__strip_zeros"}([1]);
    is(@$x, 1, q|@$x = 1|);
    is($x->[0], 1, q|$x->[0] = 1|);

    # don't strip non-zero elements
    $x = &{$CALC."::__strip_zeros"}([0, 1]);
    is(@$x, 2, q|@$x = 2|);
    is($x->[0], 0, q|$x->[0] = 0|);
    is($x->[1], 1, q|$x->[1] = 1|);
    $x = &{$CALC."::__strip_zeros"}([0, 1, 2]);
    is(@$x, 3, q|@$x = 3|);
    is($x->[0], 0, q|$x->[0] = 0|);
    is($x->[1], 1, q|$x->[1] = 1|);
    is($x->[2], 2, q|$x->[2] = 2|);

    # but strip leading zeros
    $x = &{$CALC."::__strip_zeros"}([0, 1, 2, 0]);
    is(@$x, 3, q|@$x = 3|);
    is($x->[0], 0, q|$x->[0] = 0|);
    is($x->[1], 1, q|$x->[1] = 1|);
    is($x->[2], 2, q|$x->[2] = 2|);

    $x = &{$CALC."::__strip_zeros"}([0, 1, 2, 0, 0]);
    is(@$x, 3, q|@$x = 3|);
    is($x->[0], 0, q|$x->[0] = 0|);
    is($x->[1], 1, q|$x->[1] = 1|);
    is($x->[2], 2, q|$x->[2] = 2|);

    $x = &{$CALC."::__strip_zeros"}([0, 1, 2, 0, 0, 0]);
    is(@$x, 3, q|@$x = 3|);
    is($x->[0], 0, q|$x->[0] = 0|);
    is($x->[1], 1, q|$x->[1] = 1|);
    is($x->[2], 2, q|$x->[2] = 2|);

    # collapse multiple zeros
    $x = &{$CALC."::__strip_zeros"}([0, 0, 0, 0]);
    is(@$x, 1, q|@$x = 1|);
    is($x->[0], 0, q|$x->[0] = 0|);
}

# done

1;
