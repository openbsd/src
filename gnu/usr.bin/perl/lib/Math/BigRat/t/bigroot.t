#!/usr/bin/perl -w

# Test broot function (and bsqrt() function, since it is used by broot()).

# It is too slow to be simple included in bigfltpm.inc, where it would get
# executed 3 times.

# But it is better to test the numerical functionality, instead of not testing
# it at all.

use Test::More;
use strict;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/bigroot.t//i;
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

  plan tests => 8 * 2;
  }

use Math::BigFloat;
use Math::BigInt;

my $cl = "Math::BigFloat";
my $c = "Math::BigInt";

# 2 ** 240 = 
# 1766847064778384329583297500742918515827483896875618958121606201292619776

test_broot ('2','240', 8, undef,   '1073741824');
test_broot ('2','240', 9, undef,   '106528681.3099908308759836475139583940127');
test_broot ('2','120', 9, undef,   '10321.27324073880096577298929482324664787');
test_broot ('2','120', 17, undef,   '133.3268493632747279600707813049418888729');

test_broot ('2','120', 8, undef,   '32768');
test_broot ('2','60', 8, undef,   '181.0193359837561662466161566988413540569');
test_broot ('2','60', 9, undef,   '101.5936673259647663841091609134277286651');
test_broot ('2','60', 17, undef,   '11.54672461623965153271017217302844672562');

sub test_broot
  {
  my ($x,$n,$y,$scale,$result) = @_;

  my $s = $scale || 'undef';
  is ($cl->new($x)->bpow($n)->broot($y,$scale),$result, "Try: $cl $x->bpow($n)->broot($y,$s) == $result");
  $result =~ s/\..*//;
  is ($c->new($x)->bpow($n)->broot($y,$scale),$result, "Try: $c $x->bpow($n)->broot($y,$s) == $result");
  }

