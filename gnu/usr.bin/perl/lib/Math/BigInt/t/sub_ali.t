#!/usr/bin/perl -w

# test that the new alias names work

use Test::More;
use strict;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/sub_ali.t//i;
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

  plan tests => 6;
  }

use Math::BigInt::Subclass;

use vars qw/$CL $x/;
$CL = 'Math::BigInt::Subclass';

require 'alias.inc';

