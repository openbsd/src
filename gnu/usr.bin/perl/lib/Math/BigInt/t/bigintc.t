#!/usr/bin/perl -w

use strict;
use Test;

BEGIN 
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib'; # for running manually
  }

use Math::BigInt::Calc;

BEGIN
  {
  my $additional = 0;
  $additional = 27 if $Math::BigInt::Calc::VERSION > 0.18;
  plan tests => 80 + $additional;
  }

# testing of Math::BigInt::Calc, primarily for interface/api and not for the
# math functionality

my $C = 'Math::BigInt::Calc';	# pass classname to sub's

# _new and _str
my $x = $C->_new(\"123"); my $y = $C->_new(\"321");
ok (ref($x),'ARRAY'); ok (${$C->_str($x)},123); ok (${$C->_str($y)},321);

###############################################################################
# _add, _sub, _mul, _div
ok (${$C->_str($C->_add($x,$y))},444);
ok (${$C->_str($C->_sub($x,$y))},123);
ok (${$C->_str($C->_mul($x,$y))},39483);
ok (${$C->_str($C->_div($x,$y))},123);

###############################################################################
# check that mul/div doesn't change $y
# and returns the same reference, not something new
ok (${$C->_str($C->_mul($x,$y))},39483);
ok (${$C->_str($x)},39483); ok (${$C->_str($y)},321);

ok (${$C->_str($C->_div($x,$y))},123);
ok (${$C->_str($x)},123); ok (${$C->_str($y)},321);

$x = $C->_new(\"39483");
my ($x1,$r1) = $C->_div($x,$y);
ok ("$x1","$x");
$C->_inc($x1);
ok ("$x1","$x");
ok (${$C->_str($r1)},'0');

$x = $C->_new(\"39483");	# reset

###############################################################################
my $z = $C->_new(\"2");
ok (${$C->_str($C->_add($x,$z))},39485);
my ($re,$rr) = $C->_div($x,$y);

ok (${$C->_str($re)},123); ok (${$C->_str($rr)},2);

# is_zero, _is_one, _one, _zero
ok ($C->_is_zero($x),0);
ok ($C->_is_one($x),0);

ok ($C->_is_one($C->_one()),1); ok ($C->_is_one($C->_zero()),0);
ok ($C->_is_zero($C->_zero()),1); ok ($C->_is_zero($C->_one()),0);

# is_odd, is_even
ok ($C->_is_odd($C->_one()),1); ok ($C->_is_odd($C->_zero()),0);
ok ($C->_is_even($C->_one()),0); ok ($C->_is_even($C->_zero()),1);

# _digit
$x = $C->_new(\"123456789");
ok ($C->_digit($x,0),9);
ok ($C->_digit($x,1),8);
ok ($C->_digit($x,2),7);
ok ($C->_digit($x,-1),1);
ok ($C->_digit($x,-2),2);
ok ($C->_digit($x,-3),3);

# _copy
$x = $C->_new(\"12356");
ok (${$C->_str($C->_copy($x))},12356);

# _zeros
$x = $C->_new(\"1256000000"); ok ($C->_zeros($x),6);
$x = $C->_new(\"152"); ok ($C->_zeros($x),0);
$x = $C->_new(\"123000"); ok ($C->_zeros($x),3); 

# _lsft, _rsft
$x = $C->_new(\"10"); $y = $C->_new(\"3"); 
ok (${$C->_str($C->_lsft($x,$y,10))},10000);
$x = $C->_new(\"20"); $y = $C->_new(\"3"); 
ok (${$C->_str($C->_lsft($x,$y,10))},20000);

$x = $C->_new(\"128"); $y = $C->_new(\"4");
ok (${$C->_str($C->_lsft($x,$y,2))}, 128 << 4);

$x = $C->_new(\"1000"); $y = $C->_new(\"3"); 
ok (${$C->_str($C->_rsft($x,$y,10))},1);
$x = $C->_new(\"20000"); $y = $C->_new(\"3"); 
ok (${$C->_str($C->_rsft($x,$y,10))},20);
$x = $C->_new(\"256"); $y = $C->_new(\"4");
ok (${$C->_str($C->_rsft($x,$y,2))},256 >> 4);

# _acmp
$x = $C->_new(\"123456789");
$y = $C->_new(\"987654321");
ok ($C->_acmp($x,$y),-1);
ok ($C->_acmp($y,$x),1);
ok ($C->_acmp($x,$x),0);
ok ($C->_acmp($y,$y),0);

# _div
$x = $C->_new(\"3333"); $y = $C->_new(\"1111");
ok (${$C->_str(scalar $C->_div($x,$y))},3);
$x = $C->_new(\"33333"); $y = $C->_new(\"1111"); ($x,$y) = $C->_div($x,$y);
ok (${$C->_str($x)},30); ok (${$C->_str($y)},3);
$x = $C->_new(\"123"); $y = $C->_new(\"1111"); 
($x,$y) = $C->_div($x,$y); ok (${$C->_str($x)},0); ok (${$C->_str($y)},123);

# _num
$x = $C->_new(\"12345"); $x = $C->_num($x); ok (ref($x)||'',''); ok ($x,12345);

# _sqrt
$x = $C->_new(\"144"); ok (${$C->_str($C->_sqrt($x))},'12');

# _fac
$x = $C->_new(\"0"); ok (${$C->_str($C->_fac($x))},'1');
$x = $C->_new(\"1"); ok (${$C->_str($C->_fac($x))},'1');
$x = $C->_new(\"2"); ok (${$C->_str($C->_fac($x))},'2');
$x = $C->_new(\"3"); ok (${$C->_str($C->_fac($x))},'6');
$x = $C->_new(\"4"); ok (${$C->_str($C->_fac($x))},'24');
$x = $C->_new(\"5"); ok (${$C->_str($C->_fac($x))},'120');
$x = $C->_new(\"10"); ok (${$C->_str($C->_fac($x))},'3628800');
$x = $C->_new(\"11"); ok (${$C->_str($C->_fac($x))},'39916800');

# _inc
$x = $C->_new(\"1000"); $C->_inc($x); ok (${$C->_str($x)},'1001');
$C->_dec($x); ok (${$C->_str($x)},'1000');

my $BL = Math::BigInt::Calc::_base_len();
$x = '1' . '0' x $BL;
$z = '1' . '0' x ($BL-1); $z .= '1';
$x = $C->_new(\$x); $C->_inc($x); ok (${$C->_str($x)},$z);

$x = '1' . '0' x $BL; $z = '9' x $BL;
$x = $C->_new(\$x); $C->_dec($x); ok (${$C->_str($x)},$z);

# should not happen:
# $x = $C->_new(\"-2"); $y = $C->_new(\"4"); ok ($C->_acmp($x,$y),-1);

# _mod
$x = $C->_new(\"1000"); $y = $C->_new(\"3");
ok (${$C->_str(scalar $C->_mod($x,$y))},1);
$x = $C->_new(\"1000"); $y = $C->_new(\"2");
ok (${$C->_str(scalar $C->_mod($x,$y))},0);

# _and, _or, _xor
$x = $C->_new(\"5"); $y = $C->_new(\"2");
ok (${$C->_str(scalar $C->_xor($x,$y))},7);
$x = $C->_new(\"5"); $y = $C->_new(\"2");
ok (${$C->_str(scalar $C->_or($x,$y))},7);
$x = $C->_new(\"5"); $y = $C->_new(\"3");
ok (${$C->_str(scalar $C->_and($x,$y))},1);

# _from_hex, _from_bin
ok (${$C->_str(scalar $C->_from_hex(\"0xFf"))},255);
ok (${$C->_str(scalar $C->_from_bin(\"0b10101011"))},160+11);

# _as_hex, _as_bin
ok (${$C->_str(scalar $C->_from_hex( $C->_as_hex( $C->_new(\"128"))))}, 128);
ok (${$C->_str(scalar $C->_from_bin( $C->_as_bin( $C->_new(\"128"))))}, 128);

# _check
$x = $C->_new(\"123456789");
ok ($C->_check($x),0);
ok ($C->_check(123),'123 is not a reference');

###############################################################################
# _to_large and _to_small (last since they toy with BASE_LEN etc)

exit if $Math::BigInt::Calc::VERSION < 0.19;

$C->_base_len(5,7); $x = [ qw/67890 12345 67890 12345/ ]; $C->_to_large($x);
ok (@$x,3);
ok ($x->[0], '4567890'); ok ($x->[1], '7890123'); ok ($x->[2], '123456');

$C->_base_len(5,7); $x = [ qw/54321 54321 54321 54321/ ]; $C->_to_large($x);
ok (@$x,3);
ok ($x->[0], '2154321'); ok ($x->[1], '4321543'); ok ($x->[2], '543215');

$C->_base_len(6,7); $x = [ qw/654321 654321 654321 654321/ ];
$C->_to_large($x); ok (@$x,4);
ok ($x->[0], '1654321'); ok ($x->[1], '2165432');
ok ($x->[2], '3216543'); ok ($x->[3], '654');

$C->_base_len(5,7); $C->_to_small($x); ok (@$x,5);
ok ($x->[0], '54321'); ok ($x->[1], '43216');
ok ($x->[2], '32165'); ok ($x->[3], '21654');
ok ($x->[4], '6543');

$C->_base_len(7,10); $x = [ qw/0000000 0000000 9999990 9999999/ ];
$C->_to_large($x); ok (@$x,3);
ok ($x->[0], '0000000000'); ok ($x->[1], '9999900000');
ok ($x->[2], '99999999');

$C->_base_len(7,10); $x = [ qw/0000000 0000000 9999990 9999999 99/ ];
$C->_to_large($x); ok (@$x,3);
ok ($x->[0], '0000000000'); ok ($x->[1], '9999900000');
ok ($x->[2], '9999999999');

# done

1;

