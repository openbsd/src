#!/usr/bin/perl -w

package Math::BigRat::Test;

require 5.005_02;
use strict;

use Exporter;
use Math::BigRat;
use Math::BigFloat;
use vars qw($VERSION @ISA $PACKAGE
            $accuracy $precision $round_mode $div_scale);

@ISA = qw(Exporter Math::BigRat);
$VERSION = 0.03;

use overload; 		# inherit overload from BigRat

# Globals
$accuracy = $precision = undef;
$round_mode = 'even';
$div_scale = 40;

my $class = 'Math::BigRat::Test';

#ub new
#{
#        my $proto  = shift;
#        my $class  = ref($proto) || $proto;
#
#        my $value       = shift;
#	my $a = $accuracy; $a = $_[0] if defined $_[0];
#	my $p = $precision; $p = $_[1] if defined $_[1];
#        # Store the floating point value
#        my $self = Math::BigFloat->new($value,$a,$p,$round_mode);
#        bless $self, $class;
#        $self->{'_custom'} = 1; # make sure this never goes away
#        return $self;
#}

sub bstr
  {
  # calculate a BigFloat compatible string output
  my ($x) = @_;

  $x = $class->new($x) unless ref $x;

  if ($x->{sign} !~ /^[+-]$/)           # inf, NaN etc
    {
    my $s = $x->{sign}; $s =~ s/^\+//;  # +inf => inf
    return $s;
    }

  my $s = ''; $s = $x->{sign} if $x->{sign} ne '+';     # +3 vs 3

  return $s.$x->{_n} if $x->{_d}->is_one(); 
  my $output = Math::BigFloat->new($x->{_n})->bdiv($x->{_d});
  return $s.$output->bstr();
  }

sub bsstr
  {
  # calculate a BigFloat compatible string output
  my ($x) = @_;

  $x = $class->new($x) unless ref $x;

  if ($x->{sign} !~ /^[+-]$/)           # inf, NaN etc
    {
    my $s = $x->{sign}; $s =~ s/^\+//;  # +inf => inf
    return $s;
    }

  my $s = ''; $s = $x->{sign} if $x->{sign} ne '+';     # +3 vs 3

  return $s.$x->{_n}->bsstr() if $x->{_d}->is_one(); 
  my $output = Math::BigFloat->new($x->{_n})->bdiv($x->{_d});
  return $s.$output->bsstr();
  }

1;
