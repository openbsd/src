
#
# "Tax the rat farms."
#

# The following hash values are used:
#   sign : +,-,NaN,+inf,-inf
#   _d   : denominator
#   _n   : numeraotr (value = _n/_d)
#   _a   : accuracy
#   _p   : precision
#   _f   : flags, used by MBR to flag parts of a rational as untouchable

package Math::BigRat;

require 5.005_03;
use strict;

use Exporter;
use Math::BigFloat;
use vars qw($VERSION @ISA $PACKAGE @EXPORT_OK $upgrade $downgrade
            $accuracy $precision $round_mode $div_scale);

@ISA = qw(Exporter Math::BigFloat);
@EXPORT_OK = qw();

$VERSION = '0.07';

use overload;				# inherit from Math::BigFloat

##############################################################################
# global constants, flags and accessory

use constant MB_NEVER_ROUND => 0x0001;

$accuracy = $precision = undef;
$round_mode = 'even';
$div_scale = 40;
$upgrade = undef;
$downgrade = undef;

my $nan = 'NaN';
my $class = 'Math::BigRat';
my $MBI = 'Math::BigInt';

sub isa
  {
  return 0 if $_[1] =~ /^Math::Big(Int|Float)/;		# we aren't
  UNIVERSAL::isa(@_);
  }

sub _new_from_float
  {
  # turn a single float input into a rational (like '0.1')
  my ($self,$f) = @_;

  return $self->bnan() if $f->is_nan();
  return $self->binf('-inf') if $f->{sign} eq '-inf';
  return $self->binf('+inf') if $f->{sign} eq '+inf';

  #print "f $f caller", join(' ',caller()),"\n";
  $self->{_n} = $f->{_m}->copy();			# mantissa
  $self->{_d} = $MBI->bone();
  $self->{sign} = $f->{sign}; $self->{_n}->{sign} = '+';
  if ($f->{_e}->{sign} eq '-')
    {
    # something like Math::BigRat->new('0.1');
    $self->{_d}->blsft($f->{_e}->copy()->babs(),10);	# 1 / 1 => 1/10
    }
  else
    {
    # something like Math::BigRat->new('10');
    # 1 / 1 => 10/1
    $self->{_n}->blsft($f->{_e},10) unless $f->{_e}->is_zero();	
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
      return $self->_new_from_float($n)->bnorm();
      }
    if ($n->isa('Math::BigInt'))
      {
      $self->{_n} = $n->copy();				# "mantissa" = $n
      $self->{_d} = $MBI->bone();
      $self->{sign} = $self->{_n}->{sign}; $self->{_n}->{sign} = '+';
      return $self->bnorm();
      }
    if ($n->isa('Math::BigInt::Lite'))
      {
      $self->{_n} = $MBI->new($$n);		# "mantissa" = $n
      $self->{_d} = $MBI->bone();
      $self->{sign} = $self->{_n}->{sign}; $self->{_n}->{sign} = '+';
      return $self->bnorm();
      }
    }
  return $n->copy() if ref $n;

  if (!defined $n)
    {
    $self->{_n} = $MBI->bzero();	# undef => 0
    $self->{_d} = $MBI->bone();
    $self->{sign} = '+';
    return $self->bnorm();
    }
  # string input with / delimiter
  if ($n =~ /\s*\/\s*/)
    {
    return Math::BigRat->bnan() if $n =~ /\/.*\//;	# 1/2/3 isn't valid
    return Math::BigRat->bnan() if $n =~ /\/\s*$/;	# 1/ isn't valid
    ($n,$d) = split (/\//,$n);
    # try as BigFloats first
    if (($n =~ /[\.eE]/) || ($d =~ /[\.eE]/))
      {
      # one of them looks like a float 
      $self->_new_from_float(Math::BigFloat->new($n));
      # now correct $self->{_n} due to $n
      my $f = Math::BigFloat->new($d);
      if ($f->{_e}->{sign} eq '-')
        {
	# 10 / 0.1 => 100/1
        $self->{_n}->blsft($f->{_e}->copy()->babs(),10);
        }
      else
        {
        $self->{_d}->blsft($f->{_e},10); 		# 1 / 1 => 10/1
         }
      }
    else
      {
      $self->{_n} = $MBI->new($n);
      $self->{_d} = $MBI->new($d);
      return $self->bnan() if $self->{_n}->is_nan() || $self->{_d}->is_nan();
      # inf handling is missing here
 
      $self->{sign} = $self->{_n}->{sign}; $self->{_n}->{sign} = '+';
      # if $d is negative, flip sign
      $self->{sign} =~ tr/+-/-+/ if $self->{_d}->{sign} eq '-';
      $self->{_d}->{sign} = '+';	# normalize
      }
    return $self->bnorm();
    }

  # simple string input
  if (($n =~ /[\.eE]/))
    {
    # work around bug in BigFloat that makes 1.1.2 valid
    return $self->bnan() if $n =~ /\..*\./;
    # looks like a float
    $self->_new_from_float(Math::BigFloat->new($n));
    }
  else
    {
    $self->{_n} = $MBI->new($n);
    $self->{_d} = $MBI->bone();
    $self->{sign} = $self->{_n}->{sign}; $self->{_n}->{sign} = '+';
    return $self->bnan() if $self->{sign} eq 'NaN';
    return $self->binf($self->{sign}) if $self->{sign} =~ /^[+-]inf$/;
    }
  $self->bnorm();
  }

###############################################################################

sub bstr
  {
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  if ($x->{sign} !~ /^[+-]$/)		# inf, NaN etc
    {
    my $s = $x->{sign}; $s =~ s/^\+//; 	# +inf => inf
    return $s;
    }

  my $s = ''; $s = $x->{sign} if $x->{sign} ne '+';	# +3 vs 3

  return $s.$x->{_n}->bstr() if $x->{_d}->is_one(); 
  return $s.$x->{_n}->bstr() . '/' . $x->{_d}->bstr(); 
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
  return $x->{_n}->bstr() . '/' . $x->{_d}->bstr(); 
  }

sub bnorm
  {
  # reduce the number to the shortest form and remember this (so that we
  # don't reduce again)
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  # both parts must be BigInt's
  die ("n is not $MBI but (".ref($x->{_n}).')')
    if ref($x->{_n}) ne $MBI;
  die ("d is not $MBI but (".ref($x->{_d}).')')
    if ref($x->{_d}) ne $MBI;

  # this is to prevent automatically rounding when MBI's globals are set
  $x->{_d}->{_f} = MB_NEVER_ROUND;
  $x->{_n}->{_f} = MB_NEVER_ROUND;
  # 'forget' that parts were rounded via MBI::bround() in MBF's bfround()
  $x->{_d}->{_a} = undef; $x->{_n}->{_a} = undef;
  $x->{_d}->{_p} = undef; $x->{_n}->{_p} = undef; 

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
  # used by parent class bone() to initialize number to 1
  my $self = shift;
  $self->{_n} = $MBI->bzero();
  $self->{_d} = $MBI->bzero();
  }

sub _binf
  {
  # used by parent class bone() to initialize number to 1
  my $self = shift;
  $self->{_n} = $MBI->bzero();
  $self->{_d} = $MBI->bzero();
  }

sub _bone
  {
  # used by parent class bone() to initialize number to 1
  my $self = shift;
  $self->{_n} = $MBI->bone();
  $self->{_d} = $MBI->bone();
  }

sub _bzero
  {
  # used by parent class bone() to initialize number to 1
  my $self = shift;
  $self->{_n} = $MBI->bzero();
  $self->{_d} = $MBI->bone();
  }

##############################################################################
# mul/add/div etc

sub badd
  {
  # add two rationals
  my ($self,$x,$y,$a,$p,$r) = objectify(2,@_);

  $x = $self->new($x) unless $x->isa($self);
  $y = $self->new($y) unless $y->isa($self);

  return $x->bnan() if ($x->{sign} eq 'NaN' || $y->{sign} eq 'NaN');

  #  1   1    gcd(3,4) = 1    1*3 + 1*4    7
  #  - + -                  = --------- = --                 
  #  4   3                      4*3       12

  my $gcd = $x->{_d}->bgcd($y->{_d});

  my $aa = $x->{_d}->copy();
  my $bb = $y->{_d}->copy(); 
  if ($gcd->is_one())
    {
    $bb->bdiv($gcd); $aa->bdiv($gcd);
    }
  $x->{_n}->bmul($bb); $x->{_n}->{sign} = $x->{sign};
  my $m = $y->{_n}->copy()->bmul($aa);
  $m->{sign} = $y->{sign};			# 2/1 - 2/1
  $x->{_n}->badd($m);

  $x->{_d}->bmul($y->{_d});

  # calculate new sign
  $x->{sign} = $x->{_n}->{sign}; $x->{_n}->{sign} = '+';

  $x->bnorm()->round($a,$p,$r);
  }

sub bsub
  {
  # subtract two rationals
  my ($self,$x,$y,$a,$p,$r) = objectify(2,@_);

  $x = $class->new($x) unless $x->isa($class);
  $y = $class->new($y) unless $y->isa($class);

  return $x->bnan() if ($x->{sign} eq 'NaN' || $y->{sign} eq 'NaN');
  # TODO: inf handling

  #  1   1    gcd(3,4) = 1    1*3 + 1*4    7
  #  - + -                  = --------- = --                 
  #  4   3                      4*3       12

  my $gcd = $x->{_d}->bgcd($y->{_d});

  my $aa = $x->{_d}->copy();
  my $bb = $y->{_d}->copy(); 
  if ($gcd->is_one())
    {
    $bb->bdiv($gcd); $aa->bdiv($gcd);
    }
  $x->{_n}->bmul($bb); $x->{_n}->{sign} = $x->{sign};
  my $m = $y->{_n}->copy()->bmul($aa);
  $m->{sign} = $y->{sign};			# 2/1 - 2/1
  $x->{_n}->bsub($m);

  $x->{_d}->bmul($y->{_d});
  
  # calculate new sign
  $x->{sign} = $x->{_n}->{sign}; $x->{_n}->{sign} = '+';

  $x->bnorm()->round($a,$p,$r);
  }

sub bmul
  {
  # multiply two rationals
  my ($self,$x,$y,$a,$p,$r) = objectify(2,@_);

  $x = $class->new($x) unless $x->isa($class);
  $y = $class->new($y) unless $y->isa($class);

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
  $x->{_n}->bmul($y->{_n});
  $x->{_d}->bmul($y->{_d});

  # compute new sign
  $x->{sign} = $x->{sign} eq $y->{sign} ? '+' : '-';

  $x->bnorm()->round($a,$p,$r);
  }

sub bdiv
  {
  # (dividend: BRAT or num_str, divisor: BRAT or num_str) return
  # (BRAT,BRAT) (quo,rem) or BRAT (only rem)
  my ($self,$x,$y,$a,$p,$r) = objectify(2,@_);

  $x = $class->new($x) unless $x->isa($class);
  $y = $class->new($y) unless $y->isa($class);

  return $self->_div_inf($x,$y)
   if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/) || $y->is_zero());

  # x== 0 # also: or y == 1 or y == -1
  return wantarray ? ($x,$self->bzero()) : $x if $x->is_zero();

  # TODO: list context, upgrade

  # 1     1    1   3
  # -  /  - == - * -
  # 4     3    4   1
  $x->{_n}->bmul($y->{_d});
  $x->{_d}->bmul($y->{_n});

  # compute new sign 
  $x->{sign} = $x->{sign} eq $y->{sign} ? '+' : '-';

  $x->bnorm()->round($a,$p,$r);
  $x;
  }

##############################################################################
# bdec/binc

sub bdec
  {
  # decrement value (subtract 1)
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x if $x->{sign} !~ /^[+-]$/;	# NaN, inf, -inf

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

  #$x->bsub($self->bone())->round(@r);
  }

sub binc
  {
  # increment value (add 1)
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);
  
  return $x if $x->{sign} !~ /^[+-]$/;	# NaN, inf, -inf

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

  #$x->badd($self->bone())->round(@r);
  }

##############################################################################
# is_foo methods (the rest is inherited)

sub is_int
  {
  # return true if arg (BRAT or num_str) is an integer
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return 1 if ($x->{sign} =~ /^[+-]$/) &&	# NaN and +-inf aren't
    $x->{_d}->is_one();				# 1e-1 => no integer
  0;
  }

sub is_zero
  {
  # return true if arg (BRAT or num_str) is zero
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return 1 if $x->{sign} eq '+' && $x->{_n}->is_zero();
  0;
  }

sub is_one
  {
  # return true if arg (BRAT or num_str) is +1 or -1 if signis given
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  my $sign = shift || ''; $sign = '+' if $sign ne '-';
  return 1
   if ($x->{sign} eq $sign && $x->{_n}->is_one() && $x->{_d}->is_one());
  0;
  }

sub is_odd
  {
  # return true if arg (BFLOAT or num_str) is odd or false if even
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return 1 if ($x->{sign} =~ /^[+-]$/) &&		# NaN & +-inf aren't
    ($x->{_d}->is_one() && $x->{_n}->is_odd());		# x/2 is not, but 3/1
  0;
  }

sub is_even
  {
  # return true if arg (BINT or num_str) is even or false if odd
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return 0 if $x->{sign} !~ /^[+-]$/;			# NaN & +-inf aren't
  return 1 if ($x->{_d}->is_one()			# x/3 is never
     && $x->{_n}->is_even());				# but 4/1 is
  0;
  }

BEGIN
  {
  *objectify = \&Math::BigInt::objectify;
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
  return 0;
  }

sub digit
  {
  return 0;
  }

##############################################################################
# special calc routines

sub bceil
  {
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $x unless $x->{sign} =~ /^[+-]$/;
  return $x if $x->{_d}->is_one();		# 22/1 => 22, 0/1 => 0

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

  $x->{_n}->bdiv($x->{_d});			# 22/7 => 3/1 w/ truncate
  $x->{_d}->bone();
  $x->{_n}->binc() if $x->{sign} eq '-';	# -22/7 => -4/1
  $x;
  }

sub bfac
  {
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  if (($x->{sign} eq '+') && ($x->{_d}->is_one()))
    {
    $x->{_n}->bfac();
    return $x->round(@r);
    }
  $x->bnan();
  }

sub bpow
  {
  my ($self,$x,$y,@r) = objectify(2,@_);

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
  $x;
  #$x->round(@r);
  }

sub blog
  {
  return Math::BigRat->bnan();
  }

sub bsqrt
  {
  my ($self,$x,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $x->bnan() if $x->{sign} ne '+';	# inf, NaN, -1 etc
  $x->{_d}->bsqrt($a,$p,$r);
  $x->{_n}->bsqrt($a,$p,$r);
  $x->bnorm();
  }

sub blsft
  {
  my ($self,$x,$y,$b,$a,$p,$r) = objectify(3,@_);
 
  $x->bmul( $b->copy()->bpow($y), $a,$p,$r);
  $x;
  }

sub brsft
  {
  my ($self,$x,$y,$b,$a,$p,$r) = objectify(2,@_);

  $x->bdiv( $b->copy()->bpow($y), $a,$p,$r);
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
  my ($self,$x,$y) = objectify(2,@_);

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
  my ($self,$x,$y) = objectify(2,@_);

  if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/))
    {
    # handle +-inf and NaN
    return undef if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));
    return 0 if $x->{sign} =~ /^[+-]inf$/ && $y->{sign} =~ /^[+-]inf$/;
    return +1;  # inf is always bigger
    }

  my $t = $x->{_n} * $y->{_d};
  my $u = $y->{_n} * $x->{_d};
  $t->bacmp($u);
  }

##############################################################################
# output conversation

sub as_number
  {
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $x if $x->{sign} !~ /^[+-]$/;			# NaN, inf etc 
  my $t = $x->{_n}->copy()->bdiv($x->{_d});		# 22/7 => 3
  $t->{sign} = $x->{sign};
  $t;
  }

sub import
  {
  my $self = shift;
  my $l = scalar @_;
  my $lib = ''; my @a;
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
  # let use Math::BigInt lib => 'GMP'; use Math::BigFloat; still work
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
  die ("Couldn't load $MBI: $! $@") if $@;

  # any non :constant stuff is handled by our parent, Exporter
  # even if @_ is empty, to give it a chance
  $self->SUPER::import(@a);             # for subclasses
  $self->export_to_level(1,$self,@a);   # need this, too
  }

1;

__END__

=head1 NAME

Math::BigRat - arbitrarily big rationals

=head1 SYNOPSIS

  use Math::BigRat;

  $x = Math::BigRat->new('3/7');

  print $x->bstr(),"\n";

=head1 DESCRIPTION

This is just a placeholder until the real thing is up and running. Watch this
space...

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
(usually 1e7, but this might be differen for some systems) with the least
significant digit first, while BitVect.pm uses a bit vector of base 2, most
significant bit first. Other modules might use even different means of
representing the numbers. See the respective module documentation for further
details.

=head1 METHODS

Any method not listed here is dervied from Math::BigFloat (or
Math::BigInt), so make sure you check these two modules for further
information.

=head2 new()

	$x = Math::BigRat->new('1/3');

Create a new Math::BigRat object. Input can come in various forms:

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

Returns a copy of the object as BigInt by truncating it to integer.

=head2 bfac()

	$x->bfac();

Calculates the factorial of $x. For instance:

	print Math::BigRat->new('3/1')->bfac(),"\n";	# 1*2*3
	print Math::BigRat->new('5/1')->bfac(),"\n";	# 1*2*3*4*5

Only works for integers for now.

=head2 blog()

Is not yet implemented.

=head2 bround()/round()/bfround()

Are not yet implemented.


=head1 BUGS

Some things are not yet implemented, or only implemented half-way.

=head1 LICENSE

This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=head1 SEE ALSO

L<Math::BigFloat> and L<Math::Big> as well as L<Math::BigInt::BitVect>,
L<Math::BigInt::Pari> and  L<Math::BigInt::GMP>.

The package at
L<http://search.cpan.org/search?mode=module&query=Math%3A%3ABigRat> may
contain more documentation and examples as well as testcases.

=head1 AUTHORS

(C) by Tels L<http://bloodgate.com/> 2001-2002. 

=cut
