#!/usr/bin/perl -w

use Test;
use strict;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/sub_mbf.t//i;
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
  
  plan tests => 1627
    + 6;	# + our own tests
  }

use Math::BigFloat::Subclass;

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigFloat::Subclass";
$CL = "Math::BigInt::Calc";

require 'bigfltpm.inc';	# perform same tests as bigfltpm

###############################################################################
# Now do custom tests for Subclass itself
my $ms = $class->new(23);
print "# Missing custom attribute \$ms->{_custom}" if !ok (1, $ms->{_custom});

# Check that subclass is a Math::BigFloat, but not a Math::Bigint
ok ($ms->isa('Math::BigFloat'),1);
ok ($ms->isa('Math::BigInt') || 0,0);

use Math::BigFloat;

my $bf = Math::BigFloat->new(23);		# same as other
$ms += $bf;
print "# Tried: \$ms += \$bf, got $ms" if !ok (46, $ms);
print "# Missing custom attribute \$ms->{_custom}" if !ok (1, $ms->{_custom});
print "# Wrong class: ref(\$ms) was ".ref($ms) if !ok ($class, ref($ms));
