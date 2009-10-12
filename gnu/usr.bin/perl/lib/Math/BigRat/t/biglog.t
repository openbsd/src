#!/usr/bin/perl -w

# Test blog function (and bpow, since it uses blog), as well as bexp().

use Test::More;
use strict;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/biglog.t//i;
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

  plan tests => 17;
  }

use Math::BigRat;

my $cl = "Math::BigRat";

#############################################################################
# test log($n)

# does not work yet
#is ($cl->new(2)->blog(), '0', "blog(2)");
#is ($cl->new(288)->blog(), '5',"blog(288)");
#is ($cl->new(2000)->blog(), '7', "blog(2000)");

#############################################################################
# test exp($n)

is ($cl->new(1)->bexp()->as_int(), '2', "bexp(1)");
is ($cl->new(2)->bexp()->as_int(), '7',"bexp(2)");
is ($cl->new(3)->bexp()->as_int(), '20', "bexp(3)");

# rounding not implemented yet
#is ($cl->new(3)->bexp(10), '20', "bexp(3,10)");

# $x < 0 => NaN
ok ($cl->new(-2)->blog(), 'NaN');
ok ($cl->new(-1)->blog(), 'NaN');
ok ($cl->new(-10)->blog(), 'NaN');
ok ($cl->new(-2,2)->blog(), 'NaN');

#############################################################################
# test bexp() with cached results

is ($cl->new(1)->bexp(), 
  '90933395208605785401971970164779391644753259799242' . '/' .
  '33452526613163807108170062053440751665152000000000',
  'bexp(1)');
is ($cl->new(2)->bexp(1,40), $cl->new(1)->bexp(1,45)->bpow(2,40), 'bexp(2)'); 

is ($cl->new("12.5")->bexp(1,61), $cl->new(1)->bexp(1,65)->bpow(12.5,61), 'bexp(12.5)'); 

#############################################################################
# test bexp() with big values (non-cached)

is ($cl->new(1)->bexp(1,100)->as_float(100), 
  '2.718281828459045235360287471352662497757247093699959574966967627724076630353547594571382178525166427',
 'bexp(100)');

is ($cl->new("12.5")->bexp(1,91), $cl->new(1)->bexp(1,95)->bpow(12.5,91), 
  'bexp(12.5) to 91 digits'); 

#############################################################################
# some integer results
is ($cl->new(2)->bpow(32)->blog(2),  '32', "2 ** 32");
is ($cl->new(3)->bpow(32)->blog(3),  '32', "3 ** 32");
is ($cl->new(2)->bpow(65)->blog(2),  '65', "2 ** 65");

my $x = Math::BigInt->new( '777' ) ** 256;
my $base = Math::BigInt->new( '12345678901234' );
is ($x->copy()->blog($base), 56, 'blog(777**256, 12345678901234)');

$x = Math::BigInt->new( '777' ) ** 777;
$base = Math::BigInt->new( '777' );
is ($x->copy()->blog($base), 777, 'blog(777**777, 777)');

# all done
1;

