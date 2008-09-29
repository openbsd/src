#!/usr/bin/perl -w

# Test bpi() and bexp()

use Test::More;
use strict;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/big_pi_e.t//i;
  if ($ENV{PERL_CORE})
    {
    # testing with the core distribution
    @INC = qw(../lib);
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

  plan tests => 8;
  }

use Math::BigFloat;

#############################################################################

my $pi = Math::BigFloat::bpi();

ok (!exists $pi->{_a}, 'A not set');
ok (!exists $pi->{_p}, 'P not set');

$pi = Math::BigFloat->bpi();

ok (!exists $pi->{_a}, 'A not set');
ok (!exists $pi->{_p}, 'P not set');

$pi = Math::BigFloat->bpi(10);

is ($pi->{_a}, 10, 'A set');
is ($pi->{_p}, undef, 'P not set');

#############################################################################
my $e = Math::BigFloat->new(1)->bexp();

ok (!exists $e->{_a}, 'A not set');
ok (!exists $e->{_p}, 'P not set');


