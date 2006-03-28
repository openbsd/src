#!/usr/bin/perl -w

# for testing subclassing Math::BigFloat

package Math::BigFloat::Subclass;

require 5.005_02;
use strict;

use Exporter;
use Math::BigFloat(1.38);
use vars qw($VERSION @ISA $PACKAGE
            $accuracy $precision $round_mode $div_scale);

@ISA = qw(Exporter Math::BigFloat);

$VERSION = 0.05;

use overload; 		# inherit overload from BigInt

# Globals
$accuracy = $precision = undef;
$round_mode = 'even';
$div_scale = 40;

sub new
{
        my $proto  = shift;
        my $class  = ref($proto) || $proto;

        my $value       = shift;
	my $a = $accuracy; $a = $_[0] if defined $_[0];
	my $p = $precision; $p = $_[1] if defined $_[1];
        # Store the floating point value
        my $self = Math::BigFloat->new($value,$a,$p,$round_mode);
        bless $self, $class;
        $self->{'_custom'} = 1; # make sure this never goes away
        return $self;
}

BEGIN
  {
  *objectify = \&Math::BigInt::objectify;
  # to allow Math::BigFloat::Subclass::bgcd( ... ) style calls
  *bgcd = \&Math::BigFloat::bgcd;
  *blcm = \&Math::BigFloat::blcm;
  }

1;
