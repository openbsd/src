#!/usr/bin/perl -w

use Test;
use strict;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/bare_mbi.t//i;
  if ($ENV{PERL_CORE})
    {
    # testing with the core distribution
    @INC = qw(../t/lib);
    }
  unshift @INC, qw(../lib);	# to locate the modules
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

  plan tests => 2766;
  }

use Math::BigInt lib => 'BareCalc';

print "# ",Math::BigInt->config()->{lib},"\n";

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigInt";
$CL = "Math::BigInt::BareCalc";

my $version = '1.61';	# for $VERSION tests, match current release (by hand!)

require 'bigintpm.inc';	# perform same tests as bigintpm

