#!/usr/bin/perl -w

use strict;
use Test::More tests => 51;

BEGIN { unshift @INC, 't'; }

# testing of Math::BigInt:Scalar (used by the testsuite),
# primarily for interface/api and not for the math functionality

use Math::BigInt::Scalar;

my $C = 'Math::BigInt::Scalar';	# pass classname to sub's

# _new and _str
my $x = $C->_new("123"); my $y = $C->_new("321");
is (ref($x),'SCALAR'); is ($C->_str($x),123); is ($C->_str($y),321);

# _add, _sub, _mul, _div

is ($C->_str($C->_add($x,$y)),444);
is ($C->_str($C->_sub($x,$y)),123);
is ($C->_str($C->_mul($x,$y)),39483);
is ($C->_str($C->_div($x,$y)),123);

is ($C->_str($C->_mul($x,$y)),39483);
is ($C->_str($x),39483);
is ($C->_str($y),321);
my $z = $C->_new("2");
is ($C->_str($C->_add($x,$z)),39485);
my ($re,$rr) = $C->_div($x,$y);

is ($C->_str($re),123); is ($C->_str($rr),2);

# is_zero, _is_one, _one, _zero
is ($C->_is_zero($x),0);
is ($C->_is_one($x),0);

is ($C->_is_one($C->_one()),1); is ($C->_is_one($C->_zero()),0);
is ($C->_is_zero($C->_zero()),1); is ($C->_is_zero($C->_one()),0);

# is_odd, is_even
is ($C->_is_odd($C->_one()),1); is ($C->_is_odd($C->_zero()),0);
is ($C->_is_even($C->_one()),0); is ($C->_is_even($C->_zero()),1);

# _digit
$x = $C->_new("123456789");
is ($C->_digit($x,0),9);
is ($C->_digit($x,1),8);
is ($C->_digit($x,2),7);
is ($C->_digit($x,-1),1);
is ($C->_digit($x,-2),2);
is ($C->_digit($x,-3),3);

# _copy
$x = $C->_new("12356");
is ($C->_str($C->_copy($x)),12356);

# _acmp
$x = $C->_new("123456789");
$y = $C->_new("987654321");
is ($C->_acmp($x,$y),-1);
is ($C->_acmp($y,$x),1);
is ($C->_acmp($x,$x),0);
is ($C->_acmp($y,$y),0);

# _div
$x = $C->_new("3333"); $y = $C->_new("1111");
is ($C->_str( scalar $C->_div($x,$y)),3);
$x = $C->_new("33333"); $y = $C->_new("1111"); ($x,$y) = $C->_div($x,$y);
is ($C->_str($x),30); is ($C->_str($y),3);
$x = $C->_new("123"); $y = $C->_new("1111"); 
($x,$y) = $C->_div($x,$y); is ($C->_str($x),0); is ($C->_str($y),123);

# _num
$x = $C->_new("12345"); $x = $C->_num($x); is (ref($x)||'',''); is ($x,12345);

# _len
$x = $C->_new("12345"); $x = $C->_len($x); is (ref($x)||'',''); is ($x,5);

# _and, _or, _xor
$x = $C->_new("3"); $y = $C->_new("4"); is ($C->_str( $C->_or($x,$y)),7);
$x = $C->_new("1"); $y = $C->_new("4"); is ($C->_str( $C->_xor($x,$y)),5);
$x = $C->_new("7"); $y = $C->_new("3"); is ($C->_str( $C->_and($x,$y)),3);

# _pow
$x = $C->_new("2"); $y = $C->_new("4"); is ($C->_str( $C->_pow($x,$y)),16);
$x = $C->_new("2"); $y = $C->_new("5"); is ($C->_str( $C->_pow($x,$y)),32);
$x = $C->_new("3"); $y = $C->_new("3"); is ($C->_str( $C->_pow($x,$y)),27);


# _check
$x = $C->_new("123456789");
is ($C->_check($x),0);
is ($C->_check(123),'123 is not a reference');

# done

1;
