#!/usr/bin/perl -w

use Test;
use strict;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/isa.t//i;
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

  plan tests => 7;
  }

use Math::BigInt::Subclass;
use Math::BigFloat::Subclass;
use Math::BigInt;
use Math::BigFloat;

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigInt::Subclass";
$CL = "Math::BigInt::Calc";

# Check that a subclass is still considered a BigInt
ok ($class->new(123)->isa('Math::BigInt'),1);

# ditto for plain Math::BigInt
ok (Math::BigInt->new(123)->isa('Math::BigInt'),1);

# But Math::BigFloats aren't
ok (Math::BigFloat->new(123)->isa('Math::BigInt') || 0,0);

# see what happens if we feed a Math::BigFloat into new()
$x = Math::BigInt->new(Math::BigFloat->new(123));
ok (ref($x),'Math::BigInt');
ok ($x->isa('Math::BigInt'),1);

# ditto for subclass
$x = Math::BigInt->new(Math::BigFloat->new(123));
ok (ref($x),'Math::BigInt');
ok ($x->isa('Math::BigInt'),1);

