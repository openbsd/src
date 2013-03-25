#!/usr/bin/perl -w

# test that config ( trap_nan => 1, trap_inf => 1) really works/dies

use strict;
use Test::More tests => 29;

use Math::BigRat;

my $mbi = 'Math::BigRat';
my ($cfg,$x);

foreach my $class ($mbi)
  {
  # can do and defaults are okay?
  can_ok ($class, 'config');
  is ($class->config()->{trap_nan}, 0);
  is ($class->config()->{trap_inf}, 0);

  # can set?
  $cfg = $class->config( trap_nan => 1 ); is ($cfg->{trap_nan},1);

  # can set via hash ref?
  $cfg = $class->config( { trap_nan => 1 } ); is ($cfg->{trap_nan},1);

  # also test that new() still works normally
  eval ("\$x = \$class->new('42'); \$x->bnan();");
  like ($@, qr/^Tried to set/);
  is ($x,42); 				# after new() never modified

  # can reset?
  $cfg = $class->config( trap_nan => 0 ); is ($cfg->{trap_nan},0);

  # can set?
  $cfg = $class->config( trap_inf => 1 ); is ($cfg->{trap_inf},1);
  eval ("\$x = \$class->new('4711'); \$x->binf();");
  like ($@, qr/^Tried to set/);
  is ($x,4711);				# after new() never modified

  # +$x/0 => +inf
  eval ("\$x = \$class->new('4711'); \$x->bdiv(0);");
  like ($@, qr/^Tried to set/);
  is ($x,4711);				# after new() never modified

  # -$x/0 => -inf
  eval ("\$x = \$class->new('-0815'); \$x->bdiv(0);");
  like ($@, qr/^Tried to set/);
  is ($x,-815);				# after new() never modified

  $cfg = $class->config( trap_nan => 1 );
  # 0/0 => NaN
  eval ("\$x = \$class->new('0'); \$x->bdiv(0);");
  like ($@, qr/^Tried to set/);
  is ($x,0);				# after new() never modified
  }

##############################################################################
# BigRat

$cfg = Math::BigRat->config( trap_nan => 1 );

for my $trap (qw/0.1a +inf inf -inf/)
  {
  my $x = Math::BigRat->new('7/4');

  eval ("\$x = \$mbi->new('$trap');");
  is ($x,'7/4');			# never modified since it dies
  eval ("\$x = \$mbi->new('$trap');");
  is ($x,'7/4');			# never modified since it dies
  eval ("\$x = \$mbi->new('$trap/7');");
  is ($x,'7/4');			# never modified since it dies
  }

# all tests done
