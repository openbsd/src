#!/usr/bin/perl -w

use Test;
use strict;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/bigfltpm.t//i;
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

  plan tests => 2308
	+ 5;		# own tests
  }

use Math::BigInt lib => 'Calc';
use Math::BigFloat;

use vars qw ($class $try $x $y $f @args $ans $ans1 $ans1_str $setup $CL);
$class = "Math::BigFloat";
$CL = "Math::BigInt::Calc";

ok ($class->config()->{class},$class);
ok ($class->config()->{with}, $CL);

# bug #17447: Can't call method Math::BigFloat->bsub, not a valid method
my $c = Math::BigFloat->new( '123.3' );
ok ($c->fsub(123) eq '0.3', 1); # calling fsub on a BigFloat works

# Bug until BigInt v1.86, the scale wasn't treated as a scalar:
$c = Math::BigFloat->new('0.008'); my $d = Math::BigFloat->new(3);
my $e = $c->bdiv(Math::BigFloat->new(3),$d);

ok ($e,'0.00267'); # '0.008 / 3 => 0.0027');
ok (ref($e->{_e}->[0]), ''); # 'Not a BigInt');

require 'bigfltpm.inc';	# all tests here for sharing
