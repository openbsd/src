#!/usr/bin/perl -w

# test BigFloat constants alone (w/o BigInt loading)

use strict;
use Test;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/const_mbf.t//i;
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

  plan tests => 2;
  if ($] < 5.006)
    {
    for (1..2) { skip (1,'Not supported on older Perls'); }
    exit;
    }
  } 

use Math::BigFloat ':constant';

ok (1.0 / 3.0, '0.3333333333333333333333333333333333333333');

# BigInt was not loadede with ':constant', so only floats are handled
ok (ref(2 ** 2),'');

