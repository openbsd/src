#!/usr/bin/perl -w

use strict;
use Test;

BEGIN 
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/bigints.t//i;
  if ($ENV{PERL_CORE})
    {
    @INC = qw(../t/lib);                # testing with the core distribution
    }
  unshift @INC, '../lib';       # for testing manually
  if (-d 't')
    {
    chdir 't';
    require File::Spec;
    unshift @INC, File::Spec->catdir(File::Spec->updir, $location);
    }
  else
    {
    unshift @INC, $location;
    }
  print "# INC = @INC\n";

  plan tests => 51;
  }

# testing of Math::BigInt:Scalar (used by the testsuite),
# primarily for interface/api and not for the math functionality

use Math::BigInt::Scalar;

my $C = 'Math::BigInt::Scalar';	# pass classname to sub's

# _new and _str
my $x = $C->_new("123"); my $y = $C->_new("321");
ok (ref($x),'SCALAR'); ok ($C->_str($x),123); ok ($C->_str($y),321);

# _add, _sub, _mul, _div

ok ($C->_str($C->_add($x,$y)),444);
ok ($C->_str($C->_sub($x,$y)),123);
ok ($C->_str($C->_mul($x,$y)),39483);
ok ($C->_str($C->_div($x,$y)),123);

ok ($C->_str($C->_mul($x,$y)),39483);
ok ($C->_str($x),39483);
ok ($C->_str($y),321);
my $z = $C->_new("2");
ok ($C->_str($C->_add($x,$z)),39485);
my ($re,$rr) = $C->_div($x,$y);

ok ($C->_str($re),123); ok ($C->_str($rr),2);

# is_zero, _is_one, _one, _zero
ok ($C->_is_zero($x),0);
ok ($C->_is_one($x),0);

ok ($C->_is_one($C->_one()),1); ok ($C->_is_one($C->_zero()),0);
ok ($C->_is_zero($C->_zero()),1); ok ($C->_is_zero($C->_one()),0);

# is_odd, is_even
ok ($C->_is_odd($C->_one()),1); ok ($C->_is_odd($C->_zero()),0);
ok ($C->_is_even($C->_one()),0); ok ($C->_is_even($C->_zero()),1);

# _digit
$x = $C->_new("123456789");
ok ($C->_digit($x,0),9);
ok ($C->_digit($x,1),8);
ok ($C->_digit($x,2),7);
ok ($C->_digit($x,-1),1);
ok ($C->_digit($x,-2),2);
ok ($C->_digit($x,-3),3);

# _copy
$x = $C->_new("12356");
ok ($C->_str($C->_copy($x)),12356);

# _acmp
$x = $C->_new("123456789");
$y = $C->_new("987654321");
ok ($C->_acmp($x,$y),-1);
ok ($C->_acmp($y,$x),1);
ok ($C->_acmp($x,$x),0);
ok ($C->_acmp($y,$y),0);

# _div
$x = $C->_new("3333"); $y = $C->_new("1111");
ok ($C->_str( scalar $C->_div($x,$y)),3);
$x = $C->_new("33333"); $y = $C->_new("1111"); ($x,$y) = $C->_div($x,$y);
ok ($C->_str($x),30); ok ($C->_str($y),3);
$x = $C->_new("123"); $y = $C->_new("1111"); 
($x,$y) = $C->_div($x,$y); ok ($C->_str($x),0); ok ($C->_str($y),123);

# _num
$x = $C->_new("12345"); $x = $C->_num($x); ok (ref($x)||'',''); ok ($x,12345);

# _len
$x = $C->_new("12345"); $x = $C->_len($x); ok (ref($x)||'',''); ok ($x,5);

# _and, _or, _xor
$x = $C->_new("3"); $y = $C->_new("4"); ok ($C->_str( $C->_or($x,$y)),7);
$x = $C->_new("1"); $y = $C->_new("4"); ok ($C->_str( $C->_xor($x,$y)),5);
$x = $C->_new("7"); $y = $C->_new("3"); ok ($C->_str( $C->_and($x,$y)),3);

# _pow
$x = $C->_new("2"); $y = $C->_new("4"); ok ($C->_str( $C->_pow($x,$y)),16);
$x = $C->_new("2"); $y = $C->_new("5"); ok ($C->_str( $C->_pow($x,$y)),32);
$x = $C->_new("3"); $y = $C->_new("3"); ok ($C->_str( $C->_pow($x,$y)),27);


# _check
$x = $C->_new("123456789");
ok ($C->_check($x),0);
ok ($C->_check(123),'123 is not a reference');

# done

1;

