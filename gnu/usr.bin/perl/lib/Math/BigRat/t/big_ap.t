#!/usr/bin/perl -w

use Test;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 17;
  }

use Math::BigInt;
use Math::BigFloat;
use Math::BigRat;

my $r = 'Math::BigRat';
my $proper = $r->new('12345678901234567890/2');
my $proper_inc = $r->new('12345678901234567890/2')->binc();
my $proper_dec = $r->new('12345678901234567890/2')->bdec();
my $proper_int = Math::BigInt->new('12345678901234567890');
my $proper_float = Math::BigFloat->new('12345678901234567890');
my $proper2 = $r->new('12345678901234567890');

print "# Start\n";

Math::BigInt->accuracy(3);
Math::BigFloat->accuracy(5);

my ($x,$y,$z);

##############################################################################
# new()

$z = $r->new('12345678901234567890/2');
ok ($z,$proper);

$z = $r->new('1234567890123456789E1');
ok ($z,$proper2);

$z = $r->new('12345678901234567890/1E0');
ok ($z,$proper2);
$z = $r->new('1234567890123456789e1/1');
ok ($z,$proper2);
$z = $r->new('1234567890123456789e1/1E0');
ok ($z,$proper2);

$z = $r->new($proper_int);
ok ($z,$proper2);

$z = $r->new($proper_float);
ok ($z,$proper2);

##############################################################################
# bdiv

$x = $r->new('12345678901234567890'); $y = Math::BigRat->new('2');
$z = $x->copy->bdiv($y);
ok ($z,$proper);

##############################################################################
# bmul

$x = $r->new("$proper"); $y = Math::BigRat->new('1');
$z = $x->copy->bmul($y);
ok ($z,$proper);
$z = $r->new('12345678901234567890/1E0');
ok ($z,$proper2);

$z = $r->new($proper_int);
ok ($z,$proper2);

$z = $r->new($proper_float);
ok ($z,$proper2);

##############################################################################
# bdiv

$x = $r->new('12345678901234567890'); $y = Math::BigRat->new('2');
$z = $x->copy->bdiv($y);
ok ($z,$proper);

##############################################################################
# bmul

$x = $r->new("$proper"); $y = Math::BigRat->new('1');
$z = $x->copy->bmul($y);
ok ($z,$proper);

$x = $r->new("$proper"); $y = Math::BigRat->new('2');
$z = $x->copy->bmul($y);
ok ($z,$proper2);

##############################################################################
# binc

$x = $proper->copy()->binc(); ok ($x,$proper_inc);
$x = $proper->copy()->bdec(); ok ($x,$proper_dec);

