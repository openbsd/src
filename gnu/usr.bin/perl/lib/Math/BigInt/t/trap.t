#!/usr/bin/perl -w

# test that config ( trap_nan => 1, trap_inf => 1) really works/dies

use strict;
use Test;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib'; # for running manually
  plan tests => 35;
  } 

use Math::BigInt;
use Math::BigFloat;

my $mbi = 'Math::BigInt'; my $mbf = 'Math::BigFloat';
my ($cfg,$x);

foreach my $class ($mbi, $mbf)
  {
  # can do and defaults are okay?
  ok ($class->can('config'));
  ok ($class->config()->{trap_nan}, 0);
  ok ($class->config()->{trap_inf}, 0);

  # can set?
  $cfg = $class->config( trap_nan => 1 ); ok ($cfg->{trap_nan},1);

  # also test that new() still works normally
  eval ("\$x = \$class->new('42'); \$x->bnan();");
  ok ($@ =~/^Tried to set/, 1);
  ok ($x,42); 				# after new() never modified

  # can reset?
  $cfg = $class->config( trap_nan => 0 ); ok ($cfg->{trap_nan},0);
  
  # can set?
  $cfg = $class->config( trap_inf => 1 ); ok ($cfg->{trap_inf},1);
  eval ("\$x = \$class->new('4711'); \$x->binf();");
  ok ($@ =~/^Tried to set/, 1);
  ok ($x,4711);				# after new() never modified
  
  # +$x/0 => +inf
  eval ("\$x = \$class->new('4711'); \$x->bdiv(0);");
  ok ($@ =~/^Tried to set/, 1);
  ok ($x,4711);				# after new() never modified
  
  # -$x/0 => -inf
  eval ("\$x = \$class->new('-0815'); \$x->bdiv(0);");
  ok ($@ =~/^Tried to set/, 1);
  ok ($x,-815);				# after new() never modified
  
  $cfg = $class->config( trap_nan => 1 );
  # 0/0 => NaN
  eval ("\$x = \$class->new('0'); \$x->bdiv(0);");
  ok ($@ =~/^Tried to set/, 1);
  ok ($x,0);				# after new() never modified
  }

##############################################################################
# BigInt

$x = Math::BigInt->new(2);
eval ("\$x = \$mbi->new('0.1');");
ok ($x,2);				# never modified since it dies
eval ("\$x = \$mbi->new('0a.1');");
ok ($x,2);				# never modified since it dies


##############################################################################
# BigFloat

$x = Math::BigFloat->new(2);
eval ("\$x = \$mbf->new('0.1a');");
ok ($x,2);				# never modified since it dies

# all tests done

