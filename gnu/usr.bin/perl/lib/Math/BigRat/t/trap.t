#!/usr/bin/perl -w

# test that config ( trap_nan => 1, trap_inf => 1) really works/dies

use strict;
use Test;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib'; # for running manually
  plan tests => 29;
  } 

use Math::BigRat;

my $mbi = 'Math::BigRat';
my ($cfg,$x);

foreach my $class ($mbi)
  {
  # can do and defaults are okay?
  ok ($class->can('config'));
  ok ($class->config()->{trap_nan}, 0);
  ok ($class->config()->{trap_inf}, 0);

  # can set?
  $cfg = $class->config( trap_nan => 1 ); ok ($cfg->{trap_nan},1);
  
  # can set via hash ref?
  $cfg = $class->config( { trap_nan => 1 } ); ok ($cfg->{trap_nan},1);

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
# BigRat

$cfg = Math::BigRat->config( trap_nan => 1 );

for my $trap (qw/0.1a +inf inf -inf/)
  {
  my $x = Math::BigRat->new('7/4');

  eval ("\$x = \$mbi->new('$trap');");
  print "# Got: $x\n" unless
   ok ($x,'7/4');			# never modified since it dies
  eval ("\$x = \$mbi->new('$trap');");
  print "# Got: $x\n" unless
   ok ($x,'7/4');			# never modified since it dies
  eval ("\$x = \$mbi->new('$trap/7');");
  print "# Got: $x\n" unless
   ok ($x,'7/4');			# never modified since it dies
  }

# all tests done

