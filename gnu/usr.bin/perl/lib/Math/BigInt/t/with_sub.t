#!/usr/bin/perl -w

# Test use Math::BigFloat with => 'Math::BigInt::SomeSubclass';

use Test;
use strict;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/with_sub.t//i;
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

  plan tests => 1815
	+ 1;
  }

use Math::BigFloat with => 'Math::BigInt::Subclass';

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigFloat";
$CL = "Math::BigInt::Calc";

# the with argument is ignored
ok (Math::BigFloat->config()->{with}, 'Math::BigInt::Calc');

require 'bigfltpm.inc';	# all tests here for sharing
