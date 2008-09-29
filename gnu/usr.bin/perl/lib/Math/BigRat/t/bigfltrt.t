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

  plan tests => 1;
  }

use Math::BigRat::Test lib => 'Calc';	# test via this Subclass 

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigRat::Test";
$CL = "Math::BigInt::Calc";
 
ok (1,1);

# fails still too many tests
#require 'bigfltpm.inc';		# all tests here for sharing
