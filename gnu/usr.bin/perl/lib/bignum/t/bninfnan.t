#!/usr/bin/perl -w

###############################################################################

use Test;
use strict;

BEGIN
  {
  $| = 1;
  my $location = $0; $location =~ s/biinfnan.t//i;
  if ($ENV{PERL_CORE})
    {
    @INC = qw(../lib ../lib/bignum/t); # testing with the core distribution
    }
  else
    {
    unshift @INC, '../lib';     # for testing manually
    }
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
  plan tests => 26;
  }

use bignum;

my ($x);

require "infnan.inc";

