#!/usr/bin/perl -w

# use Module(); doesn't call import() - thanx for cpan testers David. M. Town
# and Andreas Marcel Riechert for spotting it. It is fixed by the same code
# that fixes require Math::BigInt, but we make a test to be sure it really
# works.

use strict;
use Test;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/use.t//i;
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

  plan tests => 1;
  } 

my ($try,$ans,$x);

use Math::BigInt(); $x = Math::BigInt->new(1); ++$x;

ok ($x||'undef',2);

# all tests done

1;

