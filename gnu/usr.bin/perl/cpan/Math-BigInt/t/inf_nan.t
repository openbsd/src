#!/usr/bin/perl -w

# test inf/NaN handling all in one place
# Thanx to Jarkko for the excellent explanations and the tables

use Test::More;
use strict;

BEGIN
  {
  $| = 1;	
  # to locate the testing files
  my $location = $0; $location =~ s/inf_nan.t//i;
  if ($ENV{PERL_CORE})
    {
    @INC = qw(../t/lib);                # testing with the core distribution
    }
  unshift @INC, '../lib';       # for testing manually
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

	        # values    groups   operators   classes   tests 
  plan tests =>   7       * 6      * 5         * 4       * 2 +
                  7       * 6      * 2         * 4       * 1	  # bmod
;
# see bottom:		+ 4 * 10;					  # 4 classes * 10 NaN == NaN tests
  }

use Math::BigInt;
use Math::BigFloat;
use Math::BigInt::Subclass;
use Math::BigFloat::Subclass;

my @classes = 
  qw/Math::BigInt Math::BigFloat
     Math::BigInt::Subclass Math::BigFloat::Subclass
    /;

my (@args,$x,$y,$z);

# +
foreach (qw/
  -inf:-inf:-inf
  -1:-inf:-inf
  -0:-inf:-inf
  0:-inf:-inf
  1:-inf:-inf
  inf:-inf:NaN
  NaN:-inf:NaN

  -inf:-1:-inf
  -1:-1:-2
  -0:-1:-1
  0:-1:-1
  1:-1:0
  inf:-1:inf
  NaN:-1:NaN

  -inf:0:-inf
  -1:0:-1
  -0:0:0
  0:0:0
  1:0:1
  inf:0:inf
  NaN:0:NaN

  -inf:1:-inf
  -1:1:0
  -0:1:1
  0:1:1
  1:1:2
  inf:1:inf
  NaN:1:NaN

  -inf:inf:NaN
  -1:inf:inf
  -0:inf:inf
  0:inf:inf
  1:inf:inf
  inf:inf:inf
  NaN:inf:NaN

  -inf:NaN:NaN
  -1:NaN:NaN
  -0:NaN:NaN
  0:NaN:NaN
  1:NaN:NaN
  inf:NaN:NaN
  NaN:NaN:NaN
  /)
  {
  @args = split /:/,$_;
  for my $class (@classes)
    {
    $x = $class->new($args[0]);
    $y = $class->new($args[1]);
    $args[2] = '0' if $args[2] eq '-0';		# BigInt/Float hasn't got -0
    my $r = $x->badd($y);

    is($x->bstr(),$args[2],"x $class $args[0] + $args[1]");
    is($x->bstr(),$args[2],"r $class $args[0] + $args[1]");
    }
  }

# -
foreach (qw/
  -inf:-inf:NaN
  -1:-inf:inf
  -0:-inf:inf
  0:-inf:inf
  1:-inf:inf
  inf:-inf:inf
  NaN:-inf:NaN

  -inf:-1:-inf
  -1:-1:0
  -0:-1:1
  0:-1:1
  1:-1:2
  inf:-1:inf
  NaN:-1:NaN

  -inf:0:-inf
  -1:0:-1
  -0:0:-0
  0:0:0
  1:0:1
  inf:0:inf
  NaN:0:NaN

  -inf:1:-inf
  -1:1:-2
  -0:1:-1
  0:1:-1
  1:1:0
  inf:1:inf
  NaN:1:NaN

  -inf:inf:-inf
  -1:inf:-inf
  -0:inf:-inf
  0:inf:-inf
  1:inf:-inf
  inf:inf:NaN
  NaN:inf:NaN

  -inf:NaN:NaN
  -1:NaN:NaN
  -0:NaN:NaN
  0:NaN:NaN
  1:NaN:NaN
  inf:NaN:NaN
  NaN:NaN:NaN
  /)
  {
  @args = split /:/,$_;
  for my $class (@classes)
    {
    $x = $class->new($args[0]);
    $y = $class->new($args[1]);
    $args[2] = '0' if $args[2] eq '-0';		# BigInt/Float hasn't got -0
    my $r = $x->bsub($y);

    is($x->bstr(),$args[2],"x $class $args[0] - $args[1]");
    is($r->bstr(),$args[2],"r $class $args[0] - $args[1]");
    }
  }

# *
foreach (qw/
  -inf:-inf:inf
  -1:-inf:inf
  -0:-inf:NaN
  0:-inf:NaN
  1:-inf:-inf
  inf:-inf:-inf
  NaN:-inf:NaN

  -inf:-1:inf
  -1:-1:1
  -0:-1:0
  0:-1:-0
  1:-1:-1
  inf:-1:-inf
  NaN:-1:NaN

  -inf:0:NaN
  -1:0:-0
  -0:0:-0
  0:0:0
  1:0:0
  inf:0:NaN
  NaN:0:NaN

  -inf:1:-inf
  -1:1:-1
  -0:1:-0
  0:1:0
  1:1:1
  inf:1:inf
  NaN:1:NaN

  -inf:inf:-inf
  -1:inf:-inf
  -0:inf:NaN
  0:inf:NaN
  1:inf:inf
  inf:inf:inf
  NaN:inf:NaN

  -inf:NaN:NaN
  -1:NaN:NaN
  -0:NaN:NaN
  0:NaN:NaN
  1:NaN:NaN
  inf:NaN:NaN
  NaN:NaN:NaN
  /)
  {
  @args = split /:/,$_;
  for my $class (@classes)
    {
    $x = $class->new($args[0]);
    $y = $class->new($args[1]);
    $args[2] = '0' if $args[2] eq '-0';		# BigInt/Float hasn't got -0
    $args[2] = '0' if $args[2] eq '-0';	# BigInt hasn't got -0
    my $r = $x->bmul($y);

    is($x->bstr(),$args[2],"x $class $args[0] * $args[1]");
    is($r->bstr(),$args[2],"r $class $args[0] * $args[1]");
    }
  }

# /
foreach (qw/
  -inf:-inf:NaN
  -1:-inf:0
  -0:-inf:0
  0:-inf:-0
  1:-inf:-0
  inf:-inf:NaN
  NaN:-inf:NaN

  -inf:-1:inf
  -1:-1:1
  -0:-1:0
  0:-1:-0
  1:-1:-1
  inf:-1:-inf
  NaN:-1:NaN

  -inf:0:-inf
  -1:0:-inf
  -0:0:NaN
  0:0:NaN
  1:0:inf
  inf:0:inf
  NaN:0:NaN

  -inf:1:-inf
  -1:1:-1
  -0:1:-0
  0:1:0
  1:1:1
  inf:1:inf
  NaN:1:NaN

  -inf:inf:NaN
  -1:inf:-0
  -0:inf:-0
  0:inf:0
  1:inf:0
  inf:inf:NaN
  NaN:inf:NaN

  -inf:NaN:NaN
  -1:NaN:NaN
  -0:NaN:NaN
  0:NaN:NaN
  1:NaN:NaN
  inf:NaN:NaN
  NaN:NaN:NaN
  /)
  {
  @args = split /:/,$_;
  for my $class (@classes)
    {
    $x = $class->new($args[0]);
    $y = $class->new($args[1]);
    $args[2] = '0' if $args[2] eq '-0';		# BigInt/Float hasn't got -0

    my $t = $x->copy();
    my $tmod = $t->copy();

    # bdiv in scalar context
    my $r = $x->bdiv($y);
    is($x->bstr(),$args[2],"x $class $args[0] / $args[1]");
    is($r->bstr(),$args[2],"r $class $args[0] / $args[1]");

    # bmod and bdiv in list context
    my ($d,$rem) = $t->bdiv($y);

    # bdiv in list context
    is($t->bstr(),$args[2],"t $class $args[0] / $args[1]");
    is($d->bstr(),$args[2],"d $class $args[0] / $args[1]");
    
    # bmod
    my $m = $tmod->bmod($y);

    # bmod() agrees with bdiv?
    is($m->bstr(),$rem->bstr(),"m $class $args[0] % $args[1]");
    # bmod() return agrees with set value?
    is($tmod->bstr(),$m->bstr(),"o $class $args[0] % $args[1]");

    }
  }

#############################################################################
# overloaded comparisations

# these are disabled for now, since Perl itself can't seem to make up it's
# mind what NaN actually is, see [perl #33106].

#
#foreach my $c (@classes)
#  {
#  my $x = $c->bnan();
#  my $y = $c->bnan();		# test with two different objects, too
#  my $a = $c->bzero();
#
#  is ($x == $y, undef, 'NaN == NaN: undef');
#  is ($x != $y, 1, 'NaN != NaN: 1');
#  
#  is ($x == $x, undef, 'NaN == NaN: undef');
#  is ($x != $x, 1, 'NaN != NaN: 1');
#  
#  is ($a != $x, 1, '0 != NaN: 1');
#  is ($a == $x, undef, '0 == NaN: undef');
#
#  is ($a < $x, undef, '0 < NaN: undef');
#  is ($a <= $x, undef, '0 <= NaN: undef');
#  is ($a >= $x, undef, '0 >= NaN: undef');
#  is ($a > $x, undef, '0 > NaN: undef');
#  }

# All done.
