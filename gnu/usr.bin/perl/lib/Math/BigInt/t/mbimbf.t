#!/usr/bin/perl -w

# test rounding, accuracy, precicion and fallback, round_mode and mixing
# of classes

use strict;
use Test;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/mbimbf.t//i;
  if ($ENV{PERL_CORE})
    {
    @INC = qw(../lib); 		# testing with the core distribution
    }
  else
    {
    unshift @INC, '../lib';	# for testing manually
    }
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

  plan tests => 617 
    + 16;		# own tests
  }

use Math::BigInt 1.60;
use Math::BigFloat 1.35;

use vars qw/$mbi $mbf/;

$mbi = 'Math::BigInt';
$mbf = 'Math::BigFloat';

require 'mbimbf.inc';

# some tests that won't work with subclasses, since the things are only
# garantied in the Math::BigInt/BigFloat (unless subclass chooses to support
# this)

Math::BigInt->round_mode('even');	# reset for tests
Math::BigFloat->round_mode('even');	# reset for tests

ok ($Math::BigInt::rnd_mode,'even');
ok ($Math::BigFloat::rnd_mode,'even');

my $x = eval '$mbi->round_mode("huhmbi");';
ok ($@ =~ /^Unknown round mode huhmbi at/);

$x = eval '$mbf->round_mode("huhmbf");';
ok ($@ =~ /^Unknown round mode huhmbf at/);

# old way (now with test for validity)
$x = eval '$Math::BigInt::rnd_mode = "huhmbi";';
ok ($@ =~ /^Unknown round mode huhmbi at/);
$x = eval '$Math::BigFloat::rnd_mode = "huhmbf";';
ok ($@ =~ /^Unknown round mode huhmbf at/);
# see if accessor also changes old variable
$mbi->round_mode('odd'); ok ($Math::BigInt::rnd_mode,'odd');
$mbf->round_mode('odd'); ok ($Math::BigInt::rnd_mode,'odd');

foreach my $class (qw/Math::BigInt Math::BigFloat/)
  {
  ok ($class->accuracy(5),5);		# set A
  ok_undef ($class->precision());	# and now P must be cleared
  ok ($class->precision(5),5);		# set P
  ok_undef ($class->accuracy());	# and now A must be cleared
  }

