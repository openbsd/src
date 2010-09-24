#!/usr/bin/perl -w

use Test::More;
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

  plan tests => 4;
  }

# first load BigInt with Calc
use Math::BigInt lib => 'Calc';

# BigFloat will remember that we loaded Calc
require Math::BigFloat;
is (Math::BigFloat::config()->{lib}, 'Math::BigInt::Calc', 'BigFloat got Calc');

# now load BigInt again with a different lib
Math::BigInt->import( lib => 'BareCalc' );

# and finally test that BigFloat knows about BareCalc

is (Math::BigFloat::config()->{lib}, 'Math::BigInt::BareCalc', 'BigFloat was notified');

# See that Math::BigFloat supports "only"
eval "Math::BigFloat->import('only' => 'Calc')";
is (Math::BigFloat::config()->{lib}, 'Math::BigInt::Calc', '"only" worked');

# See that Math::BigFloat supports "try"
eval "Math::BigFloat->import('try' => 'BareCalc')";
is (Math::BigFloat::config()->{lib}, 'Math::BigInt::BareCalc', '"try" worked');

