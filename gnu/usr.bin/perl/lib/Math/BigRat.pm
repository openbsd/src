
#
# "Tax the rat farms." - Lord Vetinari
#

# The following hash values are used:
#   sign : +,-,NaN,+inf,-inf
#   _d   : denominator
#   _n   : numeraotr (value = _n/_d)
#   _a   : accuracy
#   _p   : precision
#   _f   : flags, used by MBR to flag parts of a rational as untouchable
# You should not look at the innards of a BigRat - use the methods for this.

package Math::BigRat;

require 5.005_03;
use strict;

require Exporter;
use Math::BigFloat;
use vars qw($VERSION @ISA $PACKAGE $upgrade $downgrade
            $accuracy $precision $round_mode $div_scale $_trap_nan $_trap_inf);

@ISA = qw(Exporter Math::BigFloat);

$VERSION = '0.12';

use overload;			# inherit from Math::BigFloat

BEGIN { *objectify = \&Math::BigInt::objectify; }

##############################################################################
# global constants, flags and accessory

$accuracy = $precision = undef;
$round_mode = 'even';
$div_scale = 40;
$upgrade = undef;
$downgrade = undef;

# these are internally, and not to be used from the outside

use constant MB_NEVER_ROUND => 0x0001;

$_trap_nan = 0;                         # are NaNs ok? set w/ config()
$_trap_inf = 0;                         # are infs ok? set w/ config()

my $nan = 'NaN';
my $MBI = 'Math::BigInt';
my $CALC = 'Math::BigInt::Calc';
my $class = 'Math::BigRat';
my $IMPORT = 0;

sub isa
  {
  return 0 if $_[1] =~ /^Math::Big(Int|Float)/;		# we aren't
  UNIVERSAL::isa(@_);
  }

sub BEGIN
  {
  *AUTOLOAD = \&Math::BigFloat::AUTOLOAD;
  }

sub _new_from_float
  {
  # turn a single float input into a rational number (like '0.1')
  my ($self,$f) = @_;

  return $self->bnan() if $f->is_nan();
  return $self->binf($f->{sign}) if $f->{sign} =~ /^[+-]inf$/;

  local $Math::BigInt::accuracy = undef;
  local $Math::BigInt::precision = undef;
  $self->{_n} = $MBI->new($CALC->_str ( $f->{_m} ),undef,undef);# mantissa
  $self->{_d} = $MBI->bone();
  $self->{sign} = $f->{sign} || '+';
  if ($f->{_es} eq '-')
    {
    # something like Math::BigRat->new('0.1');
    # 1 / 1 => 1/10
    $self->{_d}->blsft( $MBI->new($CALC->_str ( $f->{_e} )),10);	
    }
  else
    {
    # something like Math::BigRat->new('10');
    # 1 / 1 => 10/1
    $self->{_n}->blsft( $MBI->new($CALC->_str($f->{_e})),10) unless 
      $CALC->_is_zero($f->{_e});	
    }
  $self;
  }

sub new
  {
  # create a Math::BigRat
  my $class = shift;

  my ($n,$d) = shift;

  my $self = { }; bless $self,$class;
 
  # input like (BigInt,BigInt) or (BigFloat,BigFloat) not handled yet

  if ((!defined $d) && (ref $n) && (!$n->isa('Math::BigRat')))
    {
    if ($n->isa('Math::BigFloat'))
      {
      $self->_new_from_float($n);
      }
    if ($n->isa('Math::BigInt'))
      {
      # TODO: trap NaN, inf
      $self->{_n} = $n->copy();				# "mantissa" = $n
      $self->{_d} = $MBI->bone();
      $self->{sign} = $self->{_n}->{sign}; $self->{_n}->{sign} = '+';
      }
    if ($n->isa('Math::BigInt::Lite'))
      {
      # TODO: trap NaN, inf
      $self->{sign} = '+'; $self->{sign} = '-' if $$n < 0;
      $self->{_n} = $MBI->new(abs($$n),undef,undef);	# "mantissa" = $n
      $self->{_d} = $MBI->bone();
      }
    return $self->bnorm();
    }
  return $n->copy() if ref $n;

  if (!defined $n)
    {
    $self->{_n} = $MBI->bzero();			# undef => 0
    $self->{_d} = $MBI->bone();
    $self->{sign} = '+';
    return $self->bnorm();
    }
  # string input with / delimiter
  if ($n =~ /\s*\/\s*/)
    {
    return $class->bnan() if $n =~ /\/.*\//;	# 1/2/3 isn't valid
    return $class->bnan() if $n =~ /\/\s*$/;	# 1/ isn't valid
    ($n,$d) = split (/\//,$n);
    # try as BigFloats first
    if (($n =~ /[\.eE]/) || ($d =~ /[\.eE]/))
      {
      # one of them looks like a float 
      # Math::BigFloat($n,undef,undef) does not what it is supposed to do, so:
      local $Math::BigFloat::accuracy = undef;
      local $Math::BigFloat::precision = undef;
      local $Math::BigInt::accuracy = undef;
      local $Math::BigInt::precision = undef;

      my $nf = Math::BigFloat->new($n,undef,undef);
      $self->{sign} = '+';
      return $self->bnan() if $nf->is_nan();
      $self->{_n} = $MBI->new( $CALC->_str( $nf->{_m} ) );

      # now correct $self->{_n} due to $n
      my $f = Math::BigFloat->new($d,undef,undef);
      return $self->bnan() if $f->is_nan();
      $self->{_d} = $MBI->new( $CALC->_str( $f->{_m} ) );

      # calculate the difference between nE and dE
      my $diff_e = $MBI->new ($nf->exponent())->bsub ( $f->exponent);
      if ($diff_e->is_negative())
	{
        # < 0: mul d with it
        $self->{_d}->blsft($diff_e->babs(),10);
	}
      elsif (!$diff_e->is_zero())
        {
        # > 0: mul n with it
        $self->{_n}->blsft($diff_e,10);
        }
      }
    else
      {
      # both d and n are (big)ints
      $self->{_n} = $MBI->new($n,undef,undef);
      $self->{_d} = $MBI->new($d,undef,undef);
      $self->{sign} = '+';
      return $self->bnan() if $self->{_n}->{sign} eq $nan ||
                              $self->{_d}->{sign} eq $nan;
      # handle inf and NAN cases:
      if ($self->{_n}->is_inf() || $self->{_d}->is_inf())
        {
        # inf/inf => NaN
        return $self->bnan() if
	  ($self->{_n}->is_inf() && $self->{_d}->is_inf());
        if ($self->{_n}->is_inf())
	  {
	  my $s = '+'; 		# '+inf/+123' or '-inf/-123'
	  $s = '-' if substr($self->{_n}->{sign},0,1) ne $self->{_d}->{sign};
	  # +-inf/123 => +-inf
          return $self->binf($s);
	  }
        # 123/inf => 0
        return $self->bzero();
        }
 
      $self->{sign} = $self->{_n}->{sign}; $self->{_n}->babs();
      # if $d is negative, flip sign
      $self->{sign} =~ tr/+-/-+/ if $self->{_d}->{sign} eq '-';
      $self->{_d}->babs();				# normalize
      }

    return $self->bnorm();
    }

  # simple string input
  if (($n =~ /[\.eE]/))
    {
    # looks like a float, quacks like a float, so probably is a float
    # Math::BigFloat($n,undef,undef) does not what it is supposed to do, so:
    local $Math::BigFloat::accuracy = undef;
    local $Math::BigFloat::precision = undef;
    local $Math::BigInt::accuracy = undef;
    local $Math::BigInt::precision = undef;
    $self->{sign} = 'NaN';
    $self->_new_from_float(Math::BigFloat->new($n,undef,undef));
    }
  else
    {
    $self->{_n} = $MBI->new($n,undef,undef);
    $self->{_d} = $MBI->bone();
    $self->{sign} = $self->{_n}->{sign}; $self->{_n}->babs();
    return $self->bnan() if $self->{sign} eq 'NaN';
    return $self->binf($self->{sign}) if $self->{sign} =~ /^[+-]inf$/;
    }
  $self->bnorm();
  }

sub copy
  {
  my ($c,$x);
  if (@_ > 1)
    {
    # if two arguments, the first one is the class to "swallow" subclasses
    ($c,$x) = @_;
    }
  else
    {
    $x = shift;
    $c = ref($x);
    }
  return unless ref($x); # only for objects

  my $self = {}; bless $self,$c;

  $self->{sign} = $x->{sign};
  $self->{_d} = $x->{_d}->copy();
  $self->{_n} = $x->{_n}->copy();
  $self->{_a} = $x->{_a} if defined $x->{_a};
  $self->{_p} = $x->{_p} if defined $x->{_p};
  $self;
  }

##############################################################################

sub config
  {
  # return (later set?) configuration data as hash ref
  my $class = shift || 'Math::BigFloat';

  my $cfg = $class->SUPER::config(@_);

  # now we need only to override the ones that are different from our parent
  $cfg->{class} = $class;
  $cfg->{with} = $MBI;
  $cfg;
  }

##############################################################################

sub bstr
  {
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  if ($x->{sign} !~ /^[+-]$/)		# inf, NaN etc
    {
    my $s = $x->{sign}; $s =~ s/^\+//; 	# +inf => inf
    return $s;
    }

  my $s = ''; $s = $x->{sign} if $x->{sign} ne '+';	# '+3/2' => '3/2'

  return $s . $x->{_n}->bstr() if $x->{_d}->is_one();
  $s . $x->{_n}->bstr() . '/' . $x->{_d}->bstr();
  }

sub bsstr
  {
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  if ($x->{sign} !~ /^[+-]$/)		# inf, NaN etc
    {
    my $s = $x->{sign}; $s =~ s/^\+//; 	# +inf => inf
    return $s;
    }
  
  my $s = ''; $s = $x->{sign} if $x->{sign} ne '+';	# +3 vs 3
  $s . $x->{_n}->bstr() . '/' . $x->{_d}->bstr(); 
  }

sub bnorm
  {
  # reduce the number to the shortest form and remember this (so that we
  # don't reduce again)
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  # both parts must be BigInt's (or whatever we are using today)
  if (ref($x->{_n}) ne $MBI)
    {
    require Carp; Carp::croak ("n is not $MBI but (".ref($x->{_n}).')');
    }
  if (ref($x->{_d}) ne $MBI)
    {
    require Carp; Carp::croak ("d is not $MBI but (".ref($x->{_d}).')');
    }

  # this is to prevent automatically rounding when MBI's globals are set
  $x->{_d}->{_f} = MB_NEVER_ROUND;
  $x->{_n}->{_f} = MB_NEVER_ROUND;
  # 'forget' that parts were rounded via MBI::bround() in MBF's bfround()
  delete $x->{_d}->{_a}; delete $x->{_n}->{_a};
  delete $x->{_d}->{_p}; delete $x->{_n}->{_p}; 

  # no normalize for NaN, inf etc.
  return $x if $x->{sign} !~ /^[+-]$/;

  # normalize zeros to 0/1
  if (($x->{sign} =~ /^[+-]$/) &&
      ($x->{_n}->is_zero()))
    {
    $x->{sign} = '+';					# never -0
    $x->{_d} = $MBI->bone() unless $x->{_d}->is_one();
    return $x;
    }

  return $x if $x->{_d}->is_one();			# no need to reduce

  # reduce other numbers
  # disable upgrade in BigInt, otherwise deep recursion
  local $Math::BigInt::upgrade = undef;
  local $Math::BigInt::accuracy = undef;
  local $Math::BigInt::precision = undef;
  my $gcd = $x->{_n}->bgcd($x->{_d});

  if (!$gcd->is_one())
    {
    $x->{_n}->bdiv($gcd);
    $x->{_d}->bdiv($gcd);
    }
  $x;
  }

##############################################################################
# special values

sub _bnan
  {
  # used by parent class bnan() to initialize number to NaN
  my $self = shift;

  if ($_trap_nan)
    {
    require Carp;
    my $class = ref($self);
    Carp::croak ("Tried to set $self to NaN in $class\::_bnan()");
    }
  $self->{_n} = $MBI->bzero();
  $self->{_d} = $MBI->bzero();
  }

sub _binf
  {
  # used by parent class bone() to initialize number to +inf/-inf
  my $self = shift;

  if ($_trap_inf)
    {
    require Carp;
    my $class = ref($self);
    Carp::croak ("Tried to set $self to inf in $class\::_binf()");
    }
  $self->{_n} = $MBI->bzero();
  $self->{_d} = $MBI->bzero();
  }

sub _bone
  {
  # used by parent class bone() to initialize number to +1/-1
  my $self = shift;
  $self->{_n} = $MBI->bone();
  $self->{_d} = $MBI->bone();
  }

sub _bzero
  {
  # used by parent class bzero() to initialize number to 0
  my $self = shift;
  $self->{_n} = $MBI->bzero();
  $self->{_d} = $MBI->bone();
  }

##############################################################################
# mul/add/div etc

sub badd
  {
  # add two rational numbers

  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  $x = $self->new($x) unless $x->isa($self);
  $y = $self->new($y) unless $y->isa($self);

  return $x->bnan() if ($x->{sign} eq 'NaN' || $y->{sign} eq 'NaN');
  # TODO: inf handling

  #  1   1    gcd(3,4) = 1    1*3 + 1*4    7
  #  - + -                  = --------- = --                 
  #  4   3                      4*3       12

  # we do not compute the gcd() here, but simple do:
  #  5   7    5*3 + 7*4   41
  #  - + -  = --------- = --                 
  #  4   3       4*3      12
 
  # the gcd() calculation and reducing is then done in bnorm()

  local $Math::BigInt::accuracy = undef;
  local $Math::BigInt::precision = undef;

  $x->{_n}->bmul($y->{_d}); $x->{_n}->{sign} = $x->{sign};
  my $m = $y->{_n}->copy()->bmul($x->{_d});
  $m->{sign} = $y->{sign};			# 2/1 - 2/1
  $x->{_n}->badd($m);

  $x->{_d}->bmul($y->{_d});

  # calculate sign of result and norm our _n part
  $x->{sign} = $x->{_n}->{sign}; $x->{_n}->{sign} = '+';

  $x->bnorm()->round(@r);
  }

sub bsub
  {
  # subtract two rational numbers

  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  # flip sign of $x, call badd(), then flip sign of result
  $x->{sign} =~ tr/+-/-+/
    unless $x->{sign} eq '+' && $x->{_n}->is_zero();	# not -0
  $x->badd($y,@r);			# does norm and round
  $x->{sign} =~ tr/+-/-+/ 
    unless $x->{sign} eq '+' && $x->{_n}->is_zero();	# not -0
  $x;
  }

sub bmul
  {
  # multiply two rational numbers
  
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  $x = $self->new($x) unless $x->isa($self);
  $y = $self->new($y) unless $y->isa($self);

  return $x->bnan() if ($x->{sign} eq 'NaN' || $y->{sign} eq 'NaN');

  # inf handling
  if (($x->{sign} =~ /^[+-]inf$/) || ($y->{sign} =~ /^[+-]inf$/))
    {
    return $x->bnan() if $x->is_zero() || $y->is_zero();
    # result will always be +-inf:
    # +inf * +/+inf => +inf, -inf * -/-inf => +inf
    # +inf * -/-inf => -inf, -inf * +/+inf => -inf
    return $x->binf() if ($x->{sign} =~ /^\+/ && $y->{sign} =~ /^\+/);
    return $x->binf() if ($x->{sign} =~ /^-/ && $y->{sign} =~ /^-/);
    return $x->binf('-');
    }

  # x== 0 # also: or y == 1 or y == -1
  return wantarray ? ($x,$self->bzero()) : $x if $x->is_zero();

  # According to Knuth, this can be optimized by doingtwice gcd (for d and n)
  # and reducing in one step)

  #  1   1    2    1
  #  - * - =  -  = -
  #  4   3    12   6
  
  local $Math::BigInt::accuracy = undef;
  local $Math::BigInt::precision = undef;
  $x->{_n}->bmul($y->{_n});
  $x->{_d}->bmul($y->{_d});

  # compute new sign
  $x->{sign} = $x->{sign} eq $y->{sign} ? '+' : '-';

  $x->bnorm()->round(@r);
  }

sub bdiv
  {
  # (dividend: BRAT or num_str, divisor: BRAT or num_str) return
  # (BRAT,BRAT) (quo,rem) or BRAT (only rem)

  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  $x = $self->new($x) unless $x->isa($self);
  $y = $self->new($y) unless $y->isa($self);

  return $self->_div_inf($x,$y)
   if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/) || $y->is_zero());

  # x== 0 # also: or y == 1 or y == -1
  return wantarray ? ($x,$self->bzero()) : $x if $x->is_zero();

  # TODO: list context, upgrade

  # 1     1    1   3
  # -  /  - == - * -
  # 4     3    4   1
  
  local $Math::BigInt::accuracy = undef;
  local $Math::BigInt::precision = undef;
  $x->{_n}->bmul($y->{_d});
  $x->{_d}->bmul($y->{_n});

  # compute new sign 
  $x->{sign} = $x->{sign} eq $y->{sign} ? '+' : '-';

  $x->bnorm()->round(@r);
  $x;
  }

sub bmod
  {
  # compute "remainder" (in Perl way) of $x / $y

  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  $x = $self->new($x) unless $x->isa($self);
  $y = $self->new($y) unless $y->isa($self);

  return $self->_div_inf($x,$y)
   if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/) || $y->is_zero());

  return $self->_div_inf($x,$y)
   if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/) || $y->is_zero());

  return $x if $x->is_zero();           # 0 / 7 = 0, mod 0

  # compute $x - $y * floor($x/$y), keeping the sign of $x

  # locally disable these, since they would interfere
  local $Math::BigInt::upgrade = undef;
  local $Math::BigInt::accuracy = undef;
  local $Math::BigInt::precision = undef;

  my $u = $x->copy()->babs();
  # first, do a "normal" division ($x/$y)
  $u->{_d}->bmul($y->{_n});
  $u->{_n}->bmul($y->{_d});

  # compute floor
  if (!$u->{_d}->is_one())
    {
    $u->{_n}->bdiv($u->{_d});			# 22/7 => 3/1 w/ truncate
    # no need to set $u->{_d} to 1, since later we set it to $y->{_d}
    #$x->{_n}->binc() if $x->{sign} eq '-';	# -22/7 => -4/1
    }
  
  # compute $y * $u
  $u->{_d} = $y->{_d};			# 1 * $y->{_d}, see floor above
  $u->{_n}->bmul($y->{_n});

  my $xsign = $x->{sign}; $x->{sign} = '+';	# remember sign and make abs
  # compute $x - $u
  $x->bsub($u);
  $x->{sign} = $xsign;				# put sign back

  $x->bnorm()->round(@r);
  }

##############################################################################
# bdec/binc

sub bdec
  {
  # decrement value (subtract 1)
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x if $x->{sign} !~ /^[+-]$/;	# NaN, inf, -inf

  local $Math::BigInt::accuracy = undef;
  local $Math::BigInt::precision = undef;
  if ($x->{sign} eq '-')
    {
    $x->{_n}->badd($x->{_d});	# -5/2 => -7/2
    }
  else
    {
    if ($x->{_n}->bacmp($x->{_d}) < 0)
      {
      # 1/3 -- => -2/3
      $x->{_n} = $x->{_d} - $x->{_n};
      $x->{sign} = '-';
      }
    else
      {
      $x->{_n}->bsub($x->{_d});		# 5/2 => 3/2
      }
    }
  $x->bnorm()->round(@r);
  }

sub binc
  {
  # increment value (add 1)
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);
  
  return $x if $x->{sign} !~ /^[+-]$/;	# NaN, inf, -inf

  local $Math::BigInt::accuracy = undef;
  local $Math::BigInt::precision = undef;
  if ($x->{sign} eq '-')
    {
    if ($x->{_n}->bacmp($x->{_d}) < 0)
      {
      # -1/3 ++ => 2/3 (overflow at 0)
      $x->{_n} = $x->{_d} - $x->{_n};
      $x->{sign} = '+';
      }
    else
      {
      $x->{_n}->bsub($x->{_d});		# -5/2 => -3/2
      }
    }
  else
    {
    $x->{_n}->badd($x->{_d});	# 5/2 => 7/2
    }
  $x->bnorm()->round(@r);
  }

##############################################################################
# is_foo methods (the rest is inherited)

sub is_int
  {
  # return true if arg (BRAT or num_str) is an integer
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return 1 if ($x->{sign} =~ /^[+-]$/) &&	# NaN and +-inf aren't
    $x->{_d}->is_one();				# x/y && y != 1 => no integer
  0;
  }

sub is_zero
  {
  # return true if arg (BRAT or num_str) is zero
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return 1 if $x->{sign} eq '+' && $x->{_n}->is_zero();
  0;
  }

sub is_one
  {
  # return true if arg (BRAT or num_str) is +1 or -1 if signis given
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  my $sign = $_[2] || ''; $sign = '+' if $sign ne '-';
  return 1
   if ($x->{sign} eq $sign && $x->{_n}->is_one() && $x->{_d}->is_one());
  0;
  }

sub is_odd
  {
  # return true if arg (BFLOAT or num_str) is odd or false if even
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return 1 if ($x->{sign} =~ /^[+-]$/) &&		# NaN & +-inf aren't
    ($x->{_d}->is_one() && $x->{_n}->is_odd());		# x/2 is not, but 3/1
  0;
  }

sub is_even
  {
  # return true if arg (BINT or num_str) is even or false if odd
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return 0 if $x->{sign} !~ /^[+-]$/;			# NaN & +-inf aren't
  return 1 if ($x->{_d}->is_one()			# x/3 is never
     && $x->{_n}->is_even());				# but 4/1 is
  0;
  }

##############################################################################
# parts() and friends

sub numerator
  {
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $MBI->new($x->{sign}) if ($x->{sign} !~ /^[+-]$/);

  my $n = $x->{_n}->copy(); $n->{sign} = $x->{sign};
  $n;
  }

sub denominator
  {
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $MBI->new($x->{sign}) if ($x->{sign} !~ /^[+-]$/);
  $x->{_d}->copy(); 
  }

sub parts
  {
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return ($self->bnan(),$self->bnan()) if $x->{sign} eq 'NaN';
  return ($self->binf(),$self->binf()) if $x->{sign} eq '+inf';
  return ($self->binf('-'),$self->binf()) if $x->{sign} eq '-inf';

  my $n = $x->{_n}->copy();
  $n->{sign} = $x->{sign};
  return ($n,$x->{_d}->copy());
  }

sub length
  {
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return $nan unless $x->is_int();
  $x->{_n}->length();			# length(-123/1) => length(123)
  }

sub digit
  {
  my ($self,$x,$n) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return $nan unless $x->is_int();
  $x->{_n}->digit($n);			# digit(-123/1,2) => digit(123,2)
  }

##############################################################################
# special calc routines

sub bceil
  {
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $x unless $x->{sign} =~ /^[+-]$/;
  return $x if $x->{_d}->is_one();		# 22/1 => 22, 0/1 => 0

  local $Math::BigInt::upgrade = undef;
  local $Math::BigInt::accuracy = undef;
  local $Math::BigInt::precision = undef;
  $x->{_n}->bdiv($x->{_d});			# 22/7 => 3/1 w/ truncate
  $x->{_d}->bone();
  $x->{_n}->binc() if $x->{sign} eq '+';	# +22/7 => 4/1
  $x->{sign} = '+' if $x->{_n}->is_zero();	# -0 => 0
  $x;
  }

sub bfloor
  {
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $x unless $x->{sign} =~ /^[+-]$/;
  return $x if $x->{_d}->is_one();		# 22/1 => 22, 0/1 => 0

  local $Math::BigInt::upgrade = undef;
  local $Math::BigInt::accuracy = undef;
  local $Math::BigInt::precision = undef;
  $x->{_n}->bdiv($x->{_d});			# 22/7 => 3/1 w/ truncate
  $x->{_d}->bone();
  $x->{_n}->binc() if $x->{sign} eq '-';	# -22/7 => -4/1
  $x;
  }

sub bfac
  {
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  # if $x is an integer
  if (($x->{sign} eq '+') && ($x->{_d}->is_one()))
    {
    $x->{_n}->bfac();
    return $x->round(@r);
    }
  $x->bnan();
  }

sub bpow
  {
  # power ($x ** $y)

  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  return $x if $x->{sign} =~ /^[+-]inf$/;       # -inf/+inf ** x
  return $x->bnan() if $x->{sign} eq $nan || $y->{sign} eq $nan;
  return $x->bone(@r) if $y->is_zero();
  return $x->round(@r) if $x->is_one() || $y->is_one();
  if ($x->{sign} eq '-' && $x->{_n}->is_one() && $x->{_d}->is_one())
    {
    # if $x == -1 and odd/even y => +1/-1
    return $y->is_odd() ? $x->round(@r) : $x->babs()->round(@r);
    # my Casio FX-5500L has a bug here: -1 ** 2 is -1, but -1 * -1 is 1;
    }
  # 1 ** -y => 1 / (1 ** |y|)
  # so do test for negative $y after above's clause
 #  return $x->bnan() if $y->{sign} eq '-';
  return $x->round(@r) if $x->is_zero();  # 0**y => 0 (if not y <= 0)

  # shortcut y/1 (and/or x/1)
  if ($y->{_d}->is_one())
    {
    # shortcut for x/1 and y/1
    if ($x->{_d}->is_one())
      {
      $x->{_n}->bpow($y->{_n});		# x/1 ** y/1 => (x ** y)/1
      if ($y->{sign} eq '-')
        {
        # 0.2 ** -3 => 1/(0.2 ** 3)
        ($x->{_n},$x->{_d}) = ($x->{_d},$x->{_n});	# swap
        }
      # correct sign; + ** + => +
      if ($x->{sign} eq '-')
        {
        # - * - => +, - * - * - => -
        $x->{sign} = '+' if $y->{_n}->is_even();	
        }
      return $x->round(@r);
      }
    # x/z ** y/1
    $x->{_n}->bpow($y->{_n});		# 5/2 ** y/1 => 5 ** y / 2 ** y
    $x->{_d}->bpow($y->{_n});
    if ($y->{sign} eq '-')
      {
      # 0.2 ** -3 => 1/(0.2 ** 3)
      ($x->{_n},$x->{_d}) = ($x->{_d},$x->{_n});	# swap
      }
    # correct sign; + ** + => +
    if ($x->{sign} eq '-')
      {
      # - * - => +, - * - * - => -
      $x->{sign} = '+' if $y->{_n}->is_even();	
      }
    return $x->round(@r);
    }

  # regular calculation (this is wrong for d/e ** f/g)
  my $pow2 = $self->__one();
  my $y1 = $MBI->new($y->{_n}/$y->{_d})->babs();
  my $two = $MBI->new(2);
  while (!$y1->is_one())
    {
    $pow2->bmul($x) if $y1->is_odd();
    $y1->bdiv($two);
    $x->bmul($x);
    }
  $x->bmul($pow2) unless $pow2->is_one();
  # n ** -x => 1/n ** x
  ($x->{_d},$x->{_n}) = ($x->{_n},$x->{_d}) if $y->{sign} eq '-'; 
  $x->bnorm()->round(@r);
  }

sub blog
  {
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);

  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,$class,@_);
    }

  # blog(1,Y) => 0
  return $x->bzero() if $x->is_one() && $y->{sign} eq '+';

  # $x <= 0 => NaN
  return $x->bnan() if $x->is_zero() || $x->{sign} ne '+' || $y->{sign} ne '+';

  if ($x->is_int() && $y->is_int())
    {
    return $self->new($x->as_number()->blog($y->as_number(),@r));
    }

  # do it with floats
  $x->_new_from_float( $x->_as_float()->blog(Math::BigFloat->new("$y"),@r) );
  }

sub _as_float
  {
  my $x = shift;

  local $Math::BigFloat::upgrade = undef;
  local $Math::BigFloat::accuracy = undef;
  local $Math::BigFloat::precision = undef;
  # 22/7 => 3.142857143..
  Math::BigFloat->new($x->{_n})->bdiv($x->{_d}, $x->accuracy());
  }

sub broot
  {
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  if ($x->is_int() && $y->is_int())
    {
    return $self->new($x->as_number()->broot($y->as_number(),@r));
    }

  # do it with floats
  $x->_new_from_float( $x->_as_float()->broot($y,@r) );
  }

sub bmodpow
  {
  # set up parameters
  my ($self,$x,$y,$m,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$m,@r) = objectify(3,@_);
    }

  # $x or $y or $m are NaN or +-inf => NaN
  return $x->bnan()
   if $x->{sign} !~ /^[+-]$/ || $y->{sign} !~ /^[+-]$/ ||
   $m->{sign} !~ /^[+-]$/;

  if ($x->is_int() && $y->is_int() && $m->is_int())
    {
    return $self->new($x->as_number()->bmodpow($y->as_number(),$m,@r));
    }

  warn ("bmodpow() not fully implemented");
  $x->bnan();
  }

sub bmodinv
  {
  # set up parameters
  my ($self,$x,$y,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,@r) = objectify(2,@_);
    }

  # $x or $y are NaN or +-inf => NaN
  return $x->bnan() 
   if $x->{sign} !~ /^[+-]$/ || $y->{sign} !~ /^[+-]$/;

  if ($x->is_int() && $y->is_int())
    {
    return $self->new($x->as_number()->bmodinv($y->as_number(),@r));
    }

  warn ("bmodinv() not fully implemented");
  $x->bnan();
  }

sub bsqrt
  {
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x->bnan() if $x->{sign} !~ /^[+]/;    # NaN, -inf or < 0
  return $x if $x->{sign} eq '+inf';            # sqrt(inf) == inf
  return $x->round(@r) if $x->is_zero() || $x->is_one();

  local $Math::BigFloat::upgrade = undef;
  local $Math::BigFloat::downgrade = undef;
  local $Math::BigFloat::precision = undef;
  local $Math::BigFloat::accuracy = undef;
  local $Math::BigInt::upgrade = undef;
  local $Math::BigInt::precision = undef;
  local $Math::BigInt::accuracy = undef;

  $x->{_d} = Math::BigFloat->new($x->{_d})->bsqrt();
  $x->{_n} = Math::BigFloat->new($x->{_n})->bsqrt();

  # if sqrt(D) was not integer
  if ($x->{_d}->{_es} ne '+')
    {
    $x->{_n}->blsft($x->{_d}->exponent()->babs(),10);	# 7.1/4.51 => 7.1/45.1
    $x->{_d} = $MBI->new($CALC->_str($x->{_d}->{_m}));	# 7.1/45.1 => 71/45.1
    }
  # if sqrt(N) was not integer
  if ($x->{_n}->{_es} ne '+')
    {
    $x->{_d}->blsft($x->{_n}->exponent()->babs(),10);	# 71/45.1 => 710/45.1
    $x->{_n} = $MBI->new($CALC->_str($x->{_n}->{_m}));	# 710/45.1 => 710/451
    }
 
  # convert parts to $MBI again 
  $x->{_n} = $x->{_n}->as_number() unless $x->{_n}->isa($MBI);
  $x->{_d} = $x->{_d}->as_number() unless $x->{_d}->isa($MBI);
  $x->bnorm()->round(@r);
  }

sub blsft
  {
  my ($self,$x,$y,$b,@r) = objectify(3,@_);
 
  $b = 2 unless defined $b;
  $b = $self->new($b) unless ref ($b);
  $x->bmul( $b->copy()->bpow($y), @r);
  $x;
  }

sub brsft
  {
  my ($self,$x,$y,$b,@r) = objectify(2,@_);

  $b = 2 unless defined $b;
  $b = $self->new($b) unless ref ($b);
  $x->bdiv( $b->copy()->bpow($y), @r);
  $x;
  }

##############################################################################
# round

sub round
  {
  $_[0];
  }

sub bround
  {
  $_[0];
  }

sub bfround
  {
  $_[0];
  }

##############################################################################
# comparing

sub bcmp
  {
  # compare two signed numbers 
  
  # set up parameters
  my ($self,$x,$y) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y) = objectify(2,@_);
    }

  if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/))
    {
    # handle +-inf and NaN
    return undef if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));
    return 0 if $x->{sign} eq $y->{sign} && $x->{sign} =~ /^[+-]inf$/;
    return +1 if $x->{sign} eq '+inf';
    return -1 if $x->{sign} eq '-inf';
    return -1 if $y->{sign} eq '+inf';
    return +1;
    }
  # check sign for speed first
  return 1 if $x->{sign} eq '+' && $y->{sign} eq '-';   # does also 0 <=> -y
  return -1 if $x->{sign} eq '-' && $y->{sign} eq '+';  # does also -x <=> 0

  # shortcut
  my $xz = $x->{_n}->is_zero();
  my $yz = $y->{_n}->is_zero();
  return 0 if $xz && $yz;                               # 0 <=> 0
  return -1 if $xz && $y->{sign} eq '+';                # 0 <=> +y
  return 1 if $yz && $x->{sign} eq '+';                 # +x <=> 0
 
  my $t = $x->{_n} * $y->{_d}; $t->{sign} = $x->{sign};
  my $u = $y->{_n} * $x->{_d}; $u->{sign} = $y->{sign};
  $t->bcmp($u);
  }

sub bacmp
  {
  # compare two numbers (as unsigned)
 
  # set up parameters
  my ($self,$x,$y) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y) = objectify(2,$class,@_);
    }

  if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/))
    {
    # handle +-inf and NaN
    return undef if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));
    return 0 if $x->{sign} =~ /^[+-]inf$/ && $y->{sign} =~ /^[+-]inf$/;
    return 1 if $x->{sign} =~ /^[+-]inf$/ && $y->{sign} !~ /^[+-]inf$/;
    return -1;
    }

  my $t = $x->{_n} * $y->{_d};
  my $u = $y->{_n} * $x->{_d};
  $t->bacmp($u);
  }

##############################################################################
# output conversation

sub numify
  {
  # convert 17/8 => float (aka 2.125)
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);
 
  return $x->bstr() if $x->{sign} !~ /^[+-]$/;	# inf, NaN, etc

  # N/1 => N
  return $x->{_n}->numify() if $x->{_d}->is_one();

  # N/D
  my $neg = 1; $neg = -1 if $x->{sign} ne '+';
  $neg * $x->{_n}->numify() / $x->{_d}->numify();	# return sign * N/D
  }

sub as_number
  {
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return $x if $x->{sign} !~ /^[+-]$/;			# NaN, inf etc
 
  # need to disable these, otherwise bdiv() gives BigRat again
  local $Math::BigInt::upgrade = undef;
  local $Math::BigInt::accuracy = undef;
  local $Math::BigInt::precision = undef;
  my $t = $x->{_n}->copy()->bdiv($x->{_d});		# 22/7 => 3
  $t->{sign} = $x->{sign};
  $t;
  }

sub as_bin
  {
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return $x unless $x->is_int();

  my $s = $x->{sign}; $s = '' if $s eq '+';
  $s . $x->{_n}->as_bin();
  }

sub as_hex
  {
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return $x unless $x->is_int();

  my $s = $x->{sign}; $s = '' if $s eq '+';
  $s . $x->{_n}->as_hex();
  }

sub import
  {
  my $self = shift;
  my $l = scalar @_;
  my $lib = ''; my @a;
  $IMPORT++;

  for ( my $i = 0; $i < $l ; $i++)
    {
#    print "at $_[$i] (",$_[$i+1]||'undef',")\n";
    if ( $_[$i] eq ':constant' )
      {
      # this rest causes overlord er load to step in
      # print "overload @_\n";
      overload::constant float => sub { $self->new(shift); };
      }
#    elsif ($_[$i] eq 'upgrade')
#      {
#     # this causes upgrading
#      $upgrade = $_[$i+1];              # or undef to disable
#      $i++;
#      }
    elsif ($_[$i] eq 'downgrade')
      {
      # this causes downgrading
      $downgrade = $_[$i+1];            # or undef to disable
      $i++;
      }
    elsif ($_[$i] eq 'lib')
      {
      $lib = $_[$i+1] || '';            # default Calc
      $i++;
      }
    elsif ($_[$i] eq 'with')
      {
      $MBI = $_[$i+1] || 'Math::BigInt';        # default Math::BigInt
      $i++;
      }
    else
      {
      push @a, $_[$i];
      }
    }
  # let use Math::BigInt lib => 'GMP'; use Math::BigRat; still work
  my $mbilib = eval { Math::BigInt->config()->{lib} };
  if ((defined $mbilib) && ($MBI eq 'Math::BigInt'))
    {
    # MBI already loaded
    $MBI->import('lib',"$lib,$mbilib", 'objectify');
    }
  else
    {
    # MBI not loaded, or not with "Math::BigInt"
    $lib .= ",$mbilib" if defined $mbilib;

    if ($] < 5.006)
      {
      # Perl < 5.6.0 dies with "out of memory!" when eval() and ':constant' is
      # used in the same script, or eval inside import().
      my @parts = split /::/, $MBI;             # Math::BigInt => Math BigInt
      my $file = pop @parts; $file .= '.pm';    # BigInt => BigInt.pm
      $file = File::Spec->catfile (@parts, $file);
      eval { require $file; $MBI->import( lib => '$lib', 'objectify' ); }
      }
    else
      {
      my $rc = "use $MBI lib => '$lib', 'objectify';";
      eval $rc;
      }
    }
  if ($@)
    {
    require Carp; Carp::croak ("Couldn't load $MBI: $! $@");
    }

  $CALC = Math::BigFloat->config()->{lib};
  
  # any non :constant stuff is handled by our parent, Exporter
  # even if @_ is empty, to give it a chance
  $self->SUPER::import(@a);             # for subclasses
  $self->export_to_level(1,$self,@a);   # need this, too
  }

1;

__END__

=head1 NAME

Math::BigRat - arbitrarily big rational numbers

=head1 SYNOPSIS

	use Math::BigRat;

	my $x = Math::BigRat->new('3/7'); $x += '5/9';

	print $x->bstr(),"\n";
  	print $x ** 2,"\n";

	my $y = Math::BigRat->new('inf');
	print "$y ", ($y->is_inf ? 'is' : 'is not') , " infinity\n";

	my $z = Math::BigRat->new(144); $z->bsqrt();

=head1 DESCRIPTION

Math::BigRat complements Math::BigInt and Math::BigFloat by providing support
for arbitrarily big rational numbers.

=head2 MATH LIBRARY

Math with the numbers is done (by default) by a module called
Math::BigInt::Calc. This is equivalent to saying:

	use Math::BigRat lib => 'Calc';

You can change this by using:

	use Math::BigRat lib => 'BitVect';

The following would first try to find Math::BigInt::Foo, then
Math::BigInt::Bar, and when this also fails, revert to Math::BigInt::Calc:

	use Math::BigRat lib => 'Foo,Math::BigInt::Bar';

Calc.pm uses as internal format an array of elements of some decimal base
(usually 1e7, but this might be different for some systems) with the least
significant digit first, while BitVect.pm uses a bit vector of base 2, most
significant bit first. Other modules might use even different means of
representing the numbers. See the respective module documentation for further
details.

Currently the following replacement libraries exist, search for them at CPAN:

	Math::BigInt::BitVect
	Math::BigInt::GMP
	Math::BigInt::Pari
	Math::BigInt::FastCalc

=head1 METHODS

Any methods not listed here are dervied from Math::BigFloat (or
Math::BigInt), so make sure you check these two modules for further
information.

=head2 new()

	$x = Math::BigRat->new('1/3');

Create a new Math::BigRat object. Input can come in various forms:

	$x = Math::BigRat->new(123);				# scalars
	$x = Math::BigRat->new('inf');				# infinity
	$x = Math::BigRat->new('123.3');			# float
	$x = Math::BigRat->new('1/3');				# simple string
	$x = Math::BigRat->new('1 / 3');			# spaced
	$x = Math::BigRat->new('1 / 0.1');			# w/ floats
	$x = Math::BigRat->new(Math::BigInt->new(3));		# BigInt
	$x = Math::BigRat->new(Math::BigFloat->new('3.1'));	# BigFloat
	$x = Math::BigRat->new(Math::BigInt::Lite->new('2'));	# BigLite

=head2 numerator()

	$n = $x->numerator();

Returns a copy of the numerator (the part above the line) as signed BigInt.

=head2 denominator()
	
	$d = $x->denominator();

Returns a copy of the denominator (the part under the line) as positive BigInt.

=head2 parts()

	($n,$d) = $x->parts();

Return a list consisting of (signed) numerator and (unsigned) denominator as
BigInts.

=head2 as_number()

	$x = Math::BigRat->new('13/7');
	print $x->as_number(),"\n";		# '1'

Returns a copy of the object as BigInt trunced it to integer.

=head2 bfac()

	$x->bfac();

Calculates the factorial of $x. For instance:

	print Math::BigRat->new('3/1')->bfac(),"\n";	# 1*2*3
	print Math::BigRat->new('5/1')->bfac(),"\n";	# 1*2*3*4*5

Works currently only for integers.

=head2 blog()

Is not yet implemented.

=head2 bround()/round()/bfround()

Are not yet implemented.

=head2 bmod()

	use Math::BigRat;
	my $x = Math::BigRat->new('7/4');
	my $y = Math::BigRat->new('4/3');
	print $x->bmod($y);

Set $x to the remainder of the division of $x by $y.

=head2 is_one()

	print "$x is 1\n" if $x->is_one();

Return true if $x is exactly one, otherwise false.

=head2 is_zero()

	print "$x is 0\n" if $x->is_zero();

Return true if $x is exactly zero, otherwise false.

=head2 is_positive()

	print "$x is >= 0\n" if $x->is_positive();

Return true if $x is positive (greater than or equal to zero), otherwise
false. Please note that '+inf' is also positive, while 'NaN' and '-inf' aren't.

=head2 is_negative()

	print "$x is < 0\n" if $x->is_negative();

Return true if $x is negative (smaller than zero), otherwise false. Please
note that '-inf' is also negative, while 'NaN' and '+inf' aren't.

=head2 is_int()

	print "$x is an integer\n" if $x->is_int();

Return true if $x has a denominator of 1 (e.g. no fraction parts), otherwise
false. Please note that '-inf', 'inf' and 'NaN' aren't integer.

=head2 is_odd()

	print "$x is odd\n" if $x->is_odd();

Return true if $x is odd, otherwise false.

=head2 is_even()

	print "$x is even\n" if $x->is_even();

Return true if $x is even, otherwise false.

=head2 bceil()

	$x->bceil();

Set $x to the next bigger integer value (e.g. truncate the number to integer
and then increment it by one).

=head2 bfloor()
	
	$x->bfloor();

Truncate $x to an integer value.

=head2 bsqrt()
	
	$x->bsqrt();

Calculate the square root of $x.

=head2 config

        use Data::Dumper;

        print Dumper ( Math::BigRat->config() );
        print Math::BigRat->config()->{lib},"\n";

Returns a hash containing the configuration, e.g. the version number, lib
loaded etc. The following hash keys are currently filled in with the
appropriate information.

        key             RO/RW   Description
                                Example
        ============================================================
        lib             RO      Name of the Math library
                                Math::BigInt::Calc
        lib_version     RO      Version of 'lib'
                                0.30
        class           RO      The class of config you just called
                                Math::BigRat
        version         RO      version number of the class you used
                                0.10
        upgrade         RW      To which class numbers are upgraded
                                undef
        downgrade       RW      To which class numbers are downgraded
                                undef
        precision       RW      Global precision
                                undef
        accuracy        RW      Global accuracy
                                undef
        round_mode      RW      Global round mode
                                even
        div_scale       RW      Fallback acccuracy for div
                                40
        trap_nan        RW      Trap creation of NaN (undef = no)
                                undef
        trap_inf        RW      Trap creation of +inf/-inf (undef = no)
                                undef

By passing a reference to a hash you may set the configuration values. This
works only for values that a marked with a C<RW> above, anything else is
read-only.

=head1 BUGS

Some things are not yet implemented, or only implemented half-way:

=over 2

=item inf handling (partial)

=item NaN handling (partial)

=item rounding (not implemented except for bceil/bfloor)

=item $x ** $y where $y is not an integer

=item bmod(), blog(), bmodinv() and bmodpow() (partial)

=back

=head1 LICENSE

This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=head1 SEE ALSO

L<Math::BigFloat> and L<Math::Big> as well as L<Math::BigInt::BitVect>,
L<Math::BigInt::Pari> and  L<Math::BigInt::GMP>.

See L<http://search.cpan.org/search?dist=bignum> for a way to use
Math::BigRat.

The package at L<http://search.cpan.org/search?dist=Math%3A%3ABigRat>
may contain more documentation and examples as well as testcases.

=head1 AUTHORS

(C) by Tels L<http://bloodgate.com/> 2001, 2002, 2003, 2004.

=cut
