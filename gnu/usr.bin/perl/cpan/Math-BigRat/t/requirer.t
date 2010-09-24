#!/usr/bin/perl -w

# check that simple requiring BigRat works

use strict;
use Test::More;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/requirer.t//i;
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

my ($x);

require Math::BigRat; $x = Math::BigRat->new(1); ++$x;

is ($x, 2, '$x got successfully modified');

# all tests done

