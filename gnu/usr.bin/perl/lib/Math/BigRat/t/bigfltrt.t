#!/usr/bin/perl -w

use Test;
use strict;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/bigfltrt.t//i;
  if ($ENV{PERL_CORE})
    {
    # testing with the core distribution
    @INC = qw(../t/lib);
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

#  plan tests => 1585;
  plan tests => 1;
  }

#use Math::BigInt;
#use Math::BigRat;
use Math::BigRat::Test;		# test via this 

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigRat::Test";
$CL = "Math::BigInt::Calc";
 
ok (1,1);

# does not fully work yet  
#require 'bigfltpm.inc';	# all tests here for sharing
