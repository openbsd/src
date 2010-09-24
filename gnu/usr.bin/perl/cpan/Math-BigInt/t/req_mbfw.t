#!/usr/bin/perl -w

# check that requiring BigFloat and then calling import() works

use strict;
use Test::More;

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
require Math::BigFloat;
my $x = Math::BigFloat->new(1); ++$x;
is ($x,2, '$x is 2');

like (Math::BigFloat->config()->{with}, qr/^Math::BigInt::(Fast)?Calc\z/, 'with ignored' );

# now override
Math::BigFloat->import ( with => 'Math::BigInt::Subclass' );

# the "with" argument is ignored
like (Math::BigFloat->config()->{with}, qr/^Math::BigInt::(Fast)?Calc\z/, 'with ignored' );

# all tests done

