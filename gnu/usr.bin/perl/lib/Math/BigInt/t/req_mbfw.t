#!/usr/bin/perl -w

# check that requiring BigFloat and then calling import() works

use strict;
use Test;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/req_mbfw.t//i;
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

  plan tests => 3;
  } 

# normal require that calls import automatically (we thus have MBI afterwards)
require Math::BigFloat; my $x = Math::BigFloat->new(1);  ++$x; ok ($x,2);

ok (Math::BigFloat->config()->{with}, 'Math::BigInt::Calc' );

# now override
Math::BigFloat->import ( with => 'Math::BigInt::Subclass' );

# thw with argument is ignored
ok (Math::BigFloat->config()->{with}, 'Math::BigInt::Calc' );

# all tests done

