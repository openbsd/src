#!/usr/bin/perl -w

use Test;
use strict;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/sub_mbi.t//i;
  if ($ENV{PERL_CORE})
    {
    # testing with the core distribution
    @INC = qw(../t/lib);
    }
  unshift @INC, qw(../lib);
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

  plan tests => 2832
    + 5;	# +5 own tests
  }

use Math::BigInt::Subclass;

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigInt::Subclass";
$CL = "Math::BigInt::Calc";

my $version = '0.02';   # for $VERSION tests, match current release (by hand!)

require 'bigintpm.inc';	# perform same tests as bigintpm

###############################################################################
# Now do custom tests for Subclass itself
 
my $ms = $class->new(23);
print "# Missing custom attribute \$ms->{_custom}" if !ok (1, $ms->{_custom});

# Check that a subclass is still considered a BigInt
ok ($ms->isa('Math::BigInt'),1);

use Math::BigInt;

my $bi = Math::BigInt->new(23);		# same as other
$ms += $bi;
print "# Tried: \$ms += \$bi, got $ms" if !ok (46, $ms);
print "# Missing custom attribute \$ms->{_custom}" if !ok (1, $ms->{_custom});
print "# Wrong class: ref(\$ms) was ".ref($ms) if !ok ($class, ref($ms));
