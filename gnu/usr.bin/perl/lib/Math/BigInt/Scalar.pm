###############################################################################
# core math lib for BigInt, representing big numbers by normal int/float's
# for testing only, will fail any bignum test if range is exceeded

package Math::BigInt::Scalar;

use 5.005;
use strict;
# use warnings; # dont use warnings for older Perls

require Exporter;

use vars qw/@ISA $VERSION/;
@ISA = qw(Exporter);

$VERSION = '0.11';

##############################################################################
# global constants, flags and accessory
 
# constants for easier life
my $nan = 'NaN';

##############################################################################
# create objects from various representations

sub _new
  {
  # (string) return ref to num
  my $d = $_[1];
  my $x = $$d;	# make copy
  return \$x;
  }                                                                             

sub _zero
  {
  my $x = 0; return \$x;
  }

sub _one
  {
  my $x = 1; return \$x;
  }

sub _copy
  {
  my $x = $_[1];
  my $z = $$x;
  return \$z;
  }

# catch and throw away
sub import { }

##############################################################################
# convert back to string and number

sub _str
  {
  # make string
  return \"${$_[1]}";
  }                                                                             

sub _num
  {
  # make a number
  return ${$_[1]};
  }


##############################################################################
# actual math code

sub _add
  {
  my ($c,$x,$y) = @_;
  $$x += $$y;
  return $x;
  }                                                                             

sub _sub
  {
  my ($c,$x,$y) = @_;
  $$x -= $$y;
  return $x;
  }                                                                             

sub _mul
  {
  my ($c,$x,$y) = @_;
  $$x *= $$y;
  return $x;
  }                                                                             

sub _div
  {
  my ($c,$x,$y) = @_;

  my $u = int($$x / $$y); my $r = $$x % $$y; $$x = $u;
  return ($x,\$r) if wantarray;
  return $x;
  }                                                                             

sub _pow
  {
  my ($c,$x,$y) = @_;
  my $u = $$x ** $$y; $$x = $u;
  return $x;
  }

sub _and
  {
  my ($c,$x,$y) = @_;
  my $u = int($$x) & int($$y); $$x = $u;
  return $x;
  }

sub _xor
  {
  my ($c,$x,$y) = @_;
  my $u = int($$x) ^ int($$y); $$x = $u;
  return $x;
  }

sub _or
  {
  my ($c,$x,$y) = @_;
  my $u = int($$x) | int($$y); $$x = $u;
  return $x;
  }

sub _inc
  {
  my ($c,$x) = @_;
  my $u = int($$x)+1; $$x = $u;
  return $x;
  }

sub _dec
  {
  my ($c,$x) = @_;
  my $u = int($$x)-1; $$x = $u;
  return $x;
  }

##############################################################################
# testing

sub _acmp
  {
  my ($c,$x, $y) = @_;
  return ($$x <=> $$y);
  }

sub _len
  {
  return length("${$_[1]}");
  }

sub _digit
  {
  # return the nth digit, negative values count backward
  # 0 is the rightmost digit
  my ($c,$x,$n) = @_;
  
  $n ++;			# 0 => 1, 1 => 2
  return substr($$x,-$n,1);	# 1 => -1, -2 => 2 etc
  }

##############################################################################
# _is_* routines

sub _is_zero
  {
  # return true if arg is zero
  my ($c,$x) = @_;
  return ($$x == 0) <=> 0;
  }

sub _is_even
  {
  # return true if arg is even
  my ($c,$x) = @_;
  return (!($$x & 1)) <=> 0; 
  }

sub _is_odd
  {
  # return true if arg is odd
  my ($c,$x) = @_;
  return ($$x & 1) <=> 0;
  }

sub _is_one
  {
  # return true if arg is one
  my ($c,$x) = @_;
  return ($$x == 1) <=> 0;
  }

###############################################################################
# check routine to test internal state of corruptions

sub _check
  {
  # no checks yet, pull it out from the test suite
  my ($c,$x) = @_;
  return "$x is not a reference" if !ref($x);
  return 0;
  }

1;
__END__

=head1 NAME

Math::BigInt::Scalar - Pure Perl module to test Math::BigInt with scalars

=head1 SYNOPSIS

Provides support for big integer calculations via means of 'small' int/floats.
Only for testing purposes, since it will fail at large values. But it is simple
enough not to introduce bugs on it's own and to serve as a testbed.

=head1 DESCRIPTION

Please see Math::BigInt::Calc.

=head1 LICENSE
 
This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself. 

=head1 AUTHOR

Tels http://bloodgate.com in 2001.

=head1 SEE ALSO

L<Math::BigInt>, L<Math::BigInt::Calc>.

=cut
