#!/usr/bin/perl -w

use strict;
use Test::More tests => 359;

use Math::BigInt::FastCalc;

my ($BASE_LEN, undef, $AND_BITS, $XOR_BITS, $OR_BITS, $BASE_LEN_SMALL, $MAX_VAL) =
  Math::BigInt::FastCalc->_base_len();

print "# BASE_LEN = $BASE_LEN\n";
print "# MAX_VAL = $MAX_VAL\n";
print "# AND_BITS = $AND_BITS\n";
print "# XOR_BITS = $XOR_BITS\n";
print "# IOR_BITS = $OR_BITS\n";

# testing of Math::BigInt::FastCalc

my $C = 'Math::BigInt::FastCalc';		# pass classname to sub's

# _new and _str
my $x = $C->_new("123"); my $y = $C->_new("321");
is (ref($x),'ARRAY'); is ($C->_str($x),123); is ($C->_str($y),321);

###############################################################################
# _add, _sub, _mul, _div
is ($C->_str($C->_add($x,$y)),444);
is ($C->_str($C->_sub($x,$y)),123);
is ($C->_str($C->_mul($x,$y)),39483);
is ($C->_str($C->_div($x,$y)),123);

###############################################################################
# check that mul/div doesn't change $y
# and returns the same reference, not something new
is ($C->_str($C->_mul($x,$y)),39483);
is ($C->_str($x),39483); is ($C->_str($y),321);

is ($C->_str($C->_div($x,$y)),123);
is ($C->_str($x),123); is ($C->_str($y),321);

$x = $C->_new("39483");
my ($x1,$r1) = $C->_div($x,$y);
is ("$x1","$x");
$C->_inc($x1);
is ("$x1","$x");
is ($C->_str($r1),'0');

$x = $C->_new("39483");	# reset

###############################################################################
my $z = $C->_new("2");
is ($C->_str($C->_add($x,$z)),39485);
my ($re,$rr) = $C->_div($x,$y);

is ($C->_str($re),123); is ($C->_str($rr),2);

# is_zero, _is_one, _one, _zero
is ($C->_is_zero($x),'');
is ($C->_is_one($x),'');

is ($C->_str($C->_zero()),"0");
is ($C->_str($C->_one()),"1");

# _two() and _ten()
is ($C->_str($C->_two()),"2");
is ($C->_str($C->_ten()),"10");
is ($C->_is_ten($C->_two()),'');
is ($C->_is_two($C->_two()),1);
is ($C->_is_ten($C->_ten()),1);
is ($C->_is_two($C->_ten()),'');

is ($C->_is_one($C->_one()),1);
is ($C->_is_one($C->_two()), '');
is ($C->_is_one($C->_ten()), '');

is ($C->_is_one($C->_zero()), '');

is ($C->_is_zero($C->_zero()),1);

is ($C->_is_zero($C->_one()), '');

# is_odd, is_even
is ($C->_is_odd($C->_one()),1); is ($C->_is_odd($C->_zero()),'');
is ($C->_is_even($C->_one()), ''); is ($C->_is_even($C->_zero()),1);

# _len
for my $method (qw/_alen _len/)
  {
  $x = $C->_new("1"); is ($C->$method($x),1);
  $x = $C->_new("12"); is ($C->$method($x),2);
  $x = $C->_new("123"); is ($C->$method($x),3);
  $x = $C->_new("1234"); is ($C->$method($x),4);
  $x = $C->_new("12345"); is ($C->$method($x),5);
  $x = $C->_new("123456"); is ($C->$method($x),6);
  $x = $C->_new("1234567"); is ($C->$method($x),7);
  $x = $C->_new("12345678"); is ($C->$method($x),8);
  $x = $C->_new("123456789"); is ($C->$method($x),9);

  $x = $C->_new("8"); is ($C->$method($x),1);
  $x = $C->_new("21"); is ($C->$method($x),2);
  $x = $C->_new("321"); is ($C->$method($x),3);
  $x = $C->_new("4321"); is ($C->$method($x),4);
  $x = $C->_new("54321"); is ($C->$method($x),5);
  $x = $C->_new("654321"); is ($C->$method($x),6);
  $x = $C->_new("7654321"); is ($C->$method($x),7);
  $x = $C->_new("87654321"); is ($C->$method($x),8);
  $x = $C->_new("987654321"); is ($C->$method($x),9);

  $x = $C->_new("0"); is ($C->$method($x),1);
  $x = $C->_new("20"); is ($C->$method($x),2);
  $x = $C->_new("320"); is ($C->$method($x),3);
  $x = $C->_new("4320"); is ($C->$method($x),4);
  $x = $C->_new("54320"); is ($C->$method($x),5);
  $x = $C->_new("654320"); is ($C->$method($x),6);
  $x = $C->_new("7654320"); is ($C->$method($x),7);
  $x = $C->_new("87654320"); is ($C->$method($x),8);
  $x = $C->_new("987654320"); is ($C->$method($x),9);

  for (my $i = 1; $i < 9; $i++)
    {
    my $a = "$i" . '0' x ($i-1);
    $x = $C->_new($a);
    print "# Tried len '$a'\n" unless is ($C->_len($x),$i);
    }
  }

# _digit
$x = $C->_new("123456789");
is ($C->_digit($x,0),9);
is ($C->_digit($x,1),8);
is ($C->_digit($x,2),7);
is ($C->_digit($x,-1),1);
is ($C->_digit($x,-2),2);
is ($C->_digit($x,-3),3);

# _copy
foreach (qw/ 1 12 123 1234 12345 123456 1234567 12345678 123456789/)
  {
  $x = $C->_new("$_");
  is ($C->_str($C->_copy($x)),"$_");
  is ($C->_str($x),"$_");		# did _copy destroy original x?
  }

# _zeros
$x = $C->_new("1256000000"); is ($C->_zeros($x),6);
$x = $C->_new("152"); is ($C->_zeros($x),0);
$x = $C->_new("123000"); is ($C->_zeros($x),3);
$x = $C->_new("0"); is ($C->_zeros($x),0);

# _lsft, _rsft
$x = $C->_new("10"); $y = $C->_new("3");
is ($C->_str($C->_lsft($x,$y,10)),10000);
$x = $C->_new("20"); $y = $C->_new("3");
is ($C->_str($C->_lsft($x,$y,10)),20000);

$x = $C->_new("128"); $y = $C->_new("4");
is ($C->_str($C->_lsft($x,$y,2)), 128 << 4);

$x = $C->_new("1000"); $y = $C->_new("3");
is ($C->_str($C->_rsft($x,$y,10)),1);
$x = $C->_new("20000"); $y = $C->_new("3");
is ($C->_str($C->_rsft($x,$y,10)),20);
$x = $C->_new("256"); $y = $C->_new("4");
is ($C->_str($C->_rsft($x,$y,2)),256 >> 4);

$x = $C->_new("6411906467305339182857313397200584952398");
$y = $C->_new("45");
is ($C->_str($C->_rsft($x,$y,10)),0);

# _acmp
$x = $C->_new("123456789");
$y = $C->_new("987654321");
is ($C->_acmp($x,$y),-1);
is ($C->_acmp($y,$x),1);
is ($C->_acmp($x,$x),0);
is ($C->_acmp($y,$y),0);
$x = $C->_new("12");
$y = $C->_new("12");
is ($C->_acmp($x,$y),0);
$x = $C->_new("21");
is ($C->_acmp($x,$y),1);
is ($C->_acmp($y,$x),-1);
$x = $C->_new("123456789");
$y = $C->_new("1987654321");
is ($C->_acmp($x,$y),-1);
is ($C->_acmp($y,$x),+1);

$x = $C->_new("1234567890123456789");
$y = $C->_new("987654321012345678");
is ($C->_acmp($x,$y),1);
is ($C->_acmp($y,$x),-1);
is ($C->_acmp($x,$x),0);
is ($C->_acmp($y,$y),0);

$x = $C->_new("1234");
$y = $C->_new("987654321012345678");
is ($C->_acmp($x,$y),-1);
is ($C->_acmp($y,$x),1);
is ($C->_acmp($x,$x),0);
is ($C->_acmp($y,$y),0);

# _modinv
$x = $C->_new("8");
$y = $C->_new("5033");
my ($xmod,$sign) = $C->_modinv($x,$y);
is ($C->_str($xmod),'629');		# -629 % 5033 == 4404
is ($sign, '-');

# _div
$x = $C->_new("3333"); $y = $C->_new("1111");
is ($C->_str(scalar $C->_div($x,$y)),3);
$x = $C->_new("33333"); $y = $C->_new("1111"); ($x,$y) = $C->_div($x,$y);
is ($C->_str($x),30); is ($C->_str($y),3);
$x = $C->_new("123"); $y = $C->_new("1111");
($x,$y) = $C->_div($x,$y); is ($C->_str($x),0); is ($C->_str($y),123);

# _num
foreach (qw/1 12 123 1234 12345 1234567 12345678 123456789 1234567890/)
  {
  $x = $C->_new("$_");
  is (ref($x),'ARRAY'); is ($C->_str($x),"$_");
  $x = $C->_num($x); is (ref($x),''); is ($x,$_);
  }

# _sqrt
$x = $C->_new("144"); is ($C->_str($C->_sqrt($x)),'12');
$x = $C->_new("144000000000000"); is ($C->_str($C->_sqrt($x)),'12000000');

# _root
$x = $C->_new("81"); my $n = $C->_new("3"); 	# 4*4*4 = 64, 5*5*5 = 125
is ($C->_str($C->_root($x,$n)),'4');	# 4.xx => 4.0
$x = $C->_new("81"); $n = $C->_new("4"); 	# 3*3*3*3 == 81
is ($C->_str($C->_root($x,$n)),'3');

# _pow (and _root)
$x = $C->_new("0"); $n = $C->_new("3"); 	# 0 ** y => 0
is ($C->_str($C->_pow($x,$n)), 0);
$x = $C->_new("3"); $n = $C->_new("0"); 	# x ** 0 => 1
is ($C->_str($C->_pow($x,$n)), 1);
$x = $C->_new("1"); $n = $C->_new("3"); 	# 1 ** y => 1
is ($C->_str($C->_pow($x,$n)), 1);
$x = $C->_new("5"); $n = $C->_new("1"); 	# x ** 1 => x
is ($C->_str($C->_pow($x,$n)), 5);

$x = $C->_new("81"); $n = $C->_new("3"); 	# 81 ** 3 == 531441
is ($C->_str($C->_pow($x,$n)),81 ** 3);

is ($C->_str($C->_root($x,$n)),81);

$x = $C->_new("81");
is ($C->_str($C->_pow($x,$n)),81 ** 3);
is ($C->_str($C->_pow($x,$n)),'150094635296999121'); # 531441 ** 3 ==

is ($C->_str($C->_root($x,$n)),'531441');
is ($C->_str($C->_root($x,$n)),'81');

$x = $C->_new("81"); $n = $C->_new("14");
is ($C->_str($C->_pow($x,$n)),'523347633027360537213511521');
is ($C->_str($C->_root($x,$n)),'81');

$x = $C->_new("523347633027360537213511520");
is ($C->_str($C->_root($x,$n)),'80');

$x = $C->_new("523347633027360537213511522");
is ($C->_str($C->_root($x,$n)),'81');

my $res = [ qw/ 9 31 99 316 999 3162 9999/ ];

# 99 ** 2 = 9801, 999 ** 2 = 998001 etc
for my $i (2 .. 9)
  {
  $x = '9' x $i; $x = $C->_new($x);
  $n = $C->_new("2");
  my $rc = '9' x ($i-1). '8' . '0' x ($i-1) . '1';
  print "# _pow( ", '9' x $i, ", 2) \n" unless
   is ($C->_str($C->_pow($x,$n)),$rc);

  if ($i <= 7)
    {
    $x = '9' x $i; $x = $C->_new($x);
    $n = '9' x $i; $n = $C->_new($n);
    print "# _root( ", '9' x $i, ", ", 9 x $i, ") \n" unless
     is ($C->_str($C->_root($x,$n)),'1');

    $x = '9' x $i; $x = $C->_new($x);
    $n = $C->_new("2");
    print "# _root( ", '9' x $i, ", ", 9 x $i, ") \n" unless
     is ($C->_str($C->_root($x,$n)), $res->[$i-2]);
    }
  }

##############################################################################
# _fac
$x = $C->_new("0"); is ($C->_str($C->_fac($x)),'1');
$x = $C->_new("1"); is ($C->_str($C->_fac($x)),'1');
$x = $C->_new("2"); is ($C->_str($C->_fac($x)),'2');
$x = $C->_new("3"); is ($C->_str($C->_fac($x)),'6');
$x = $C->_new("4"); is ($C->_str($C->_fac($x)),'24');
$x = $C->_new("5"); is ($C->_str($C->_fac($x)),'120');
$x = $C->_new("10"); is ($C->_str($C->_fac($x)),'3628800');
$x = $C->_new("11"); is ($C->_str($C->_fac($x)),'39916800');
$x = $C->_new("12"); is ($C->_str($C->_fac($x)),'479001600');
$x = $C->_new("13"); is ($C->_str($C->_fac($x)),'6227020800');

# test that _fac modifies $x in place for small arguments
$x = $C->_new("3"); $C->_fac($x); is ($C->_str($x),'6');
$x = $C->_new("13"); $C->_fac($x); is ($C->_str($x),'6227020800');

##############################################################################
# _inc and _dec
foreach (qw/1 11 121 1231 12341 1234561 12345671 123456781 1234567891/)
  {
  $x = $C->_new("$_"); $C->_inc($x);
  print "# \$x = ",$C->_str($x),"\n"
   unless is ($C->_str($x),substr($_,0,length($_)-1) . '2');
  $C->_dec($x); is ($C->_str($x),$_);
  }
foreach (qw/19 119 1219 12319 1234519 12345619 123456719 1234567819/)
  {
  $x = $C->_new("$_"); $C->_inc($x);
  print "# \$x = ",$C->_str($x),"\n"
   unless is ($C->_str($x),substr($_,0,length($_)-2) . '20');
  $C->_dec($x); is ($C->_str($x),$_);
  }
foreach (qw/999 9999 99999 9999999 99999999 999999999 9999999999 99999999999/)
  {
  $x = $C->_new("$_"); $C->_inc($x);
  print "# \$x = ",$C->_str($x),"\n"
   unless is ($C->_str($x), '1' . '0' x (length($_)));
  $C->_dec($x); is ($C->_str($x),$_);
  }

$x = $C->_new("1000"); $C->_inc($x); is ($C->_str($x),'1001');
$C->_dec($x); is ($C->_str($x),'1000');

my $BL = $C -> _base_len();

$x = '1' . '0' x $BL;
$z = '1' . '0' x ($BL-1); $z .= '1';
$x = $C->_new($x); $C->_inc($x); is ($C->_str($x),$z);

$x = '1' . '0' x $BL; $z = '9' x $BL;
$x = $C->_new($x); $C->_dec($x); is ($C->_str($x),$z);

# should not happen:
# $x = $C->_new("-2"); $y = $C->_new("4"); is ($C->_acmp($x,$y),-1);

###############################################################################
# _mod
$x = $C->_new("1000"); $y = $C->_new("3");
is ($C->_str(scalar $C->_mod($x,$y)),1);
$x = $C->_new("1000"); $y = $C->_new("2");
is ($C->_str(scalar $C->_mod($x,$y)),0);

# _and, _or, _xor
$x = $C->_new("5"); $y = $C->_new("2");
is ($C->_str(scalar $C->_xor($x,$y)),7);
$x = $C->_new("5"); $y = $C->_new("2");
is ($C->_str(scalar $C->_or($x,$y)),7);
$x = $C->_new("5"); $y = $C->_new("3");
is ($C->_str(scalar $C->_and($x,$y)),1);

# _from_hex, _from_bin, _from_oct
is ($C->_str( $C->_from_hex("0xFf")),255);
is ($C->_str( $C->_from_bin("0b10101011")),160+11);
is ($C->_str( $C->_from_oct("0100")), 8*8);
is ($C->_str( $C->_from_oct("01000")), 8*8*8);
is ($C->_str( $C->_from_oct("010001")), 8*8*8*8+1);
is ($C->_str( $C->_from_oct("010007")), 8*8*8*8+7);

# _as_hex, _as_bin, as_oct
is ($C->_str( $C->_from_hex( $C->_as_hex( $C->_new("128")))), 128);
is ($C->_str( $C->_from_bin( $C->_as_bin( $C->_new("128")))), 128);
is ($C->_str( $C->_from_oct( $C->_as_oct( $C->_new("128")))), 128);

is ($C->_str( $C->_from_oct( $C->_as_oct( $C->_new("123456")))), 123456);
is ($C->_str( $C->_from_oct( $C->_as_oct( $C->_new("123456789")))), "123456789");
is ($C->_str( $C->_from_oct( $C->_as_oct( $C->_new("1234567890123")))), "1234567890123");

# _1ex
is ($C->_str($C->_1ex(0)), "1");
is ($C->_str($C->_1ex(1)), "10");
is ($C->_str($C->_1ex(2)), "100");
is ($C->_str($C->_1ex(12)), "1000000000000");
is ($C->_str($C->_1ex(16)), "10000000000000000");

# _check
$x = $C->_new("123456789");
is ($C->_check($x),0);
is ($C->_check(123),'123 is not a reference');

###############################################################################
# __strip_zeros

{
  no strict 'refs';
  # correct empty arrays
  $x = &{$C."::__strip_zeros"}([]); is (@$x,1); is ($x->[0],0);
  # don't strip single elements
  $x = &{$C."::__strip_zeros"}([0]); is (@$x,1); is ($x->[0],0);
  $x = &{$C."::__strip_zeros"}([1]); is (@$x,1); is ($x->[0],1);
  # don't strip non-zero elements
  $x = &{$C."::__strip_zeros"}([0,1]);
  is (@$x,2); is ($x->[0],0); is ($x->[1],1);
  $x = &{$C."::__strip_zeros"}([0,1,2]);
  is (@$x,3); is ($x->[0],0); is ($x->[1],1); is ($x->[2],2);

  # but strip leading zeros
  $x = &{$C."::__strip_zeros"}([0,1,2,0]);
  is (@$x,3); is ($x->[0],0); is ($x->[1],1); is ($x->[2],2);

  $x = &{$C."::__strip_zeros"}([0,1,2,0,0]);
  is (@$x,3); is ($x->[0],0); is ($x->[1],1); is ($x->[2],2);

  $x = &{$C."::__strip_zeros"}([0,1,2,0,0,0]);
  is (@$x,3); is ($x->[0],0); is ($x->[1],1); is ($x->[2],2);

  # collapse multiple zeros
  $x = &{$C."::__strip_zeros"}([0,0,0,0]);
  is (@$x,1); is ($x->[0],0);
}

# done

1;
