#!/usr/bin/perl -w

# see if using Math::BigInt and Math::BigFloat works together nicely.
# all use_lib*.t should be equivalent

use strict;
use Test;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/use_lib2.t//i;
  if ($ENV{PERL_CORE})
    {
    # testing with the core distribution
    @INC = qw(../t/lib);
    }
  unshift @INC, qw(../lib);     # to locate the modules
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

  plan tests => 2;
  } 

use Math::BigInt;
use Math::BigFloat lib => 'BareCalc';

ok (Math::BigInt->config()->{lib},'Math::BigInt::BareCalc');

ok (Math::BigFloat->new(123)->badd(123),246);

