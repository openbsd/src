#!/usr/bin/perl -w

# check that simple requiring BigFloat and then new() works

use strict;
use Test;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/req_mbfn.t//i;
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

require Math::BigFloat; my $x = Math::BigFloat->new(1);  ++$x; ok ($x,2);

# all tests done

