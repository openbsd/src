#!/usr/bin/perl -w

# test the helper math routines in Math::BigFloat

use Test::More;
use strict;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/_e_math.t//i;
  if ($ENV{PERL_CORE})
    {
    # testing with the core distribution
    @INC = qw(../lib);
    }
  unshift @INC, '../lib';
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

  plan tests => 26;
  }

use Math::BigFloat lib => 'Calc';

#############################################################################
# add

my $a = Math::BigInt::Calc->_new("123");
my $b = Math::BigInt::Calc->_new("321");

my ($x, $xs) = Math::BigFloat::_e_add($a,$b,'+','+');
is (_str($x,$xs), '+444', 'add two positive numbers');
is (_str($a,''), '444', 'a modified');

($x,$xs) = _add (123,321,'+','+');
is (_str($x,$xs), '+444', 'add two positive numbers');

($x,$xs) = _add (123,321,'+','-');
is (_str($x,$xs), '-198', 'add +x + -y');
($x,$xs) = _add (123,321,'-','+');
is (_str($x,$xs), '+198', 'add -x + +y');

($x,$xs) = _add (321,123,'-','+');
is (_str($x,$xs), '-198', 'add -x + +y');
($x,$xs) = _add (321,123,'+','-');
is (_str($x,$xs), '+198', 'add +x + -y');

($x,$xs) = _add (10,1,'+','-');
is (_str($x,$xs), '+9', 'add 10 + -1');
($x,$xs) = _add (10,1,'-','+');
is (_str($x,$xs), '-9', 'add -10 + +1');
($x,$xs) = _add (1,10,'-','+');
is (_str($x,$xs), '+9', 'add -1 + 10');
($x,$xs) = _add (1,10,'+','-');
is (_str($x,$xs), '-9', 'add 1 + -10');

#############################################################################
# sub

$a = Math::BigInt::Calc->_new("123");
$b = Math::BigInt::Calc->_new("321");
($x, $xs) = Math::BigFloat::_e_sub($b,$a,'+','+');
is (_str($x,$xs), '+198', 'sub two positive numbers');
is (_str($b,''), '198', 'a modified');

($x,$xs) = _sub (123,321,'+','-');
is (_str($x,$xs), '+444', 'sub +x + -y');
($x,$xs) = _sub (123,321,'-','+');
is (_str($x,$xs), '-444', 'sub -x + +y');

sub _add
  {
  my ($a,$b,$as,$bs) = @_;

  my $aa = Math::BigInt::Calc->_new($a);
  my $bb = Math::BigInt::Calc->_new($b);
  my ($x, $xs) = Math::BigFloat::_e_add($aa,$bb,$as,$bs);
  is (Math::BigInt::Calc->_str($x), Math::BigInt::Calc->_str($aa),
    'param0 modified');
  ($x,$xs);
  }

sub _sub
  {
  my ($a,$b,$as,$bs) = @_;

  my $aa = Math::BigInt::Calc->_new($a);
  my $bb = Math::BigInt::Calc->_new($b);
  my ($x, $xs) = Math::BigFloat::_e_sub($aa,$bb,$as,$bs);
  is (Math::BigInt::Calc->_str($x), Math::BigInt::Calc->_str($aa),
    'param0 modified');
  ($x,$xs);
  }

sub _str
  {
  my ($x,$s) = @_;

  $s . Math::BigInt::Calc->_str($x);
  }
