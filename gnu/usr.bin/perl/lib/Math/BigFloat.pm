package Math::BigFloat;

# 
# Mike grinned. 'Two down, infinity to go' - Mike Nostrus in 'Before and After'
#

# The following hash values are internally used:
#   _e: exponent (BigInt)
#   _m: mantissa (absolute BigInt)
# sign: +,-,+inf,-inf, or "NaN" if not a number
#   _a: accuracy
#   _p: precision
#   _f: flags, used to signal MBI not to touch our private parts

$VERSION = '1.42';
require 5.005;

require Exporter;
@ISA =       qw(Exporter Math::BigInt);

use strict;
# $_trap_inf and $_trap_nan are internal and should never be accessed from the outside
use vars qw/$AUTOLOAD $accuracy $precision $div_scale $round_mode $rnd_mode
	    $upgrade $downgrade $_trap_nan $_trap_inf/;
my $class = "Math::BigFloat";

use overload
'<=>'	=>	sub { $_[2] ?
                      ref($_[0])->bcmp($_[1],$_[0]) : 
                      ref($_[0])->bcmp($_[0],$_[1])},
'int'	=>	sub { $_[0]->as_number() },		# 'trunc' to bigint
;

##############################################################################
# global constants, flags and assorted stuff

# the following are public, but their usage is not recommended. Use the
# accessor methods instead.

# class constants, use Class->constant_name() to access
$round_mode = 'even'; # one of 'even', 'odd', '+inf', '-inf', 'zero' or 'trunc'
$accuracy   = undef;
$precision  = undef;
$div_scale  = 40;

$upgrade = undef;
$downgrade = undef;
my $MBI = 'Math::BigInt'; # the package we are using for our private parts
			  # changable by use Math::BigFloat with => 'package'

# the following are private and not to be used from the outside:

sub MB_NEVER_ROUND () { 0x0001; }

# are NaNs ok? (otherwise it dies when encountering an NaN) set w/ config()
$_trap_nan = 0;
# the same for infs
$_trap_inf = 0;

# constant for easier life
my $nan = 'NaN'; 

my $IMPORT = 0;                         # was import() called yet?
                                        # used to make require work

# some digits of accuracy for blog(undef,10); which we use in blog() for speed
my $LOG_10 = 
 '2.3025850929940456840179914546843642076011014886287729760333279009675726097';
my $LOG_10_A = length($LOG_10)-1;
# ditto for log(2)
my $LOG_2 = 
 '0.6931471805599453094172321214581765680755001343602552541206800094933936220';
my $LOG_2_A = length($LOG_2)-1;

##############################################################################
# the old code had $rnd_mode, so we need to support it, too

sub TIESCALAR   { my ($class) = @_; bless \$round_mode, $class; }
sub FETCH       { return $round_mode; }
sub STORE       { $rnd_mode = $_[0]->round_mode($_[1]); }

BEGIN
  {
  # when someone set's $rnd_mode, we catch this and check the value to see
  # whether it is valid or not. 
  $rnd_mode   = 'even'; tie $rnd_mode, 'Math::BigFloat'; 
  }
 
##############################################################################

# in case we call SUPER::->foo() and this wants to call modify()
# sub modify () { 0; }

{
  # valid method aliases for AUTOLOAD
  my %methods = map { $_ => 1 }  
   qw / fadd fsub fmul fdiv fround ffround fsqrt fmod fstr fsstr fpow fnorm
        fint facmp fcmp fzero fnan finf finc fdec flog ffac
	fceil ffloor frsft flsft fone flog froot
      /;
  # valid method's that can be hand-ed up (for AUTOLOAD)
  my %hand_ups = map { $_ => 1 }  
   qw / is_nan is_inf is_negative is_positive
        accuracy precision div_scale round_mode fneg fabs fnot
        objectify upgrade downgrade
	bone binf bnan bzero
      /;

  sub method_alias { return exists $methods{$_[0]||''}; } 
  sub method_hand_up { return exists $hand_ups{$_[0]||''}; } 
}

##############################################################################
# constructors

sub new 
  {
  # create a new BigFloat object from a string or another bigfloat object. 
  # _e: exponent
  # _m: mantissa
  # sign  => sign (+/-), or "NaN"

  my ($class,$wanted,@r) = @_;

  # avoid numify-calls by not using || on $wanted!
  return $class->bzero() if !defined $wanted;	# default to 0
  return $wanted->copy() if UNIVERSAL::isa($wanted,'Math::BigFloat');

  $class->import() if $IMPORT == 0;             # make require work

  my $self = {}; bless $self, $class;
  # shortcut for bigints and its subclasses
  if ((ref($wanted)) && (ref($wanted) ne $class))
    {
    $self->{_m} = $wanted->as_number();		# get us a bigint copy
    $self->{_e} = $MBI->bzero();
    $self->{_m}->babs();
    $self->{sign} = $wanted->sign();
    return $self->bnorm();
    }
  # got string
  # handle '+inf', '-inf' first
  if ($wanted =~ /^[+-]?inf$/)
    {
    return $downgrade->new($wanted) if $downgrade;

    $self->{_e} = $MBI->bzero();
    $self->{_m} = $MBI->bzero();
    $self->{sign} = $wanted;
    $self->{sign} = '+inf' if $self->{sign} eq 'inf';
    return $self->bnorm();
    }
  #print "new string '$wanted'\n";

  my ($mis,$miv,$mfv,$es,$ev) = Math::BigInt::_split(\$wanted);
  if (!ref $mis)
    {
    if ($_trap_nan)
      {
      require Carp;
      Carp::croak ("$wanted is not a number initialized to $class");
      }
    
    return $downgrade->bnan() if $downgrade;
    
    $self->{_e} = $MBI->bzero();
    $self->{_m} = $MBI->bzero();
    $self->{sign} = $nan;
    }
  else
    {
    # make integer from mantissa by adjusting exp, then convert to bigint
    # undef,undef to signal MBI that we don't need no bloody rounding
    $self->{_e} = $MBI->new("$$es$$ev",undef,undef);	# exponent
    $self->{_m} = $MBI->new("$$miv$$mfv",undef,undef); 	# create mant.

    # this is to prevent automatically rounding when MBI's globals are set
    $self->{_m}->{_f} = MB_NEVER_ROUND;
    $self->{_e}->{_f} = MB_NEVER_ROUND;

    # 3.123E0 = 3123E-3, and 3.123E-2 => 3123E-5
    $self->{_e}->bsub( $MBI->new(CORE::length($$mfv),undef,undef))
      if CORE::length($$mfv) != 0;
    $self->{sign} = $$mis;
    
    #print "$$miv$$mfv $$es$$ev\n";

    # we can only have trailing zeros on the mantissa of $$mfv eq ''
    if (CORE::length($$mfv) == 0)
      {
      my $zeros = $self->{_m}->_trailing_zeros(); # correct for trailing zeros 
      if ($zeros != 0)
        {
        $self->{_m}->brsft($zeros,10); $self->{_e}->badd($MBI->new($zeros));
        }
      }
#    else
#      {
      # for something like 0Ey, set y to 1, and -0 => +0
      $self->{sign} = '+', $self->{_e}->bone() if $self->{_m}->is_zero();
#      }
    return $self->round(@r) if !$downgrade;
    }
  # if downgrade, inf, NaN or integers go down

  if ($downgrade && $self->{_e}->{sign} eq '+')
    {
    #print "downgrading $$miv$$mfv"."E$$es$$ev";
    if ($self->{_e}->is_zero())
      {
      $self->{_m}->{sign} = $$mis;		# negative if wanted
      return $downgrade->new($self->{_m});
      }
    return $downgrade->new($self->bsstr()); 
    }
  #print "mbf new $self->{sign} $self->{_m} e $self->{_e} ",ref($self),"\n";
  $self->bnorm()->round(@r);			# first normalize, then round
  }

sub _bnan
  {
  # used by parent class bone() to initialize number to NaN
  my $self = shift;
  
  if ($_trap_nan)
    {
    require Carp;
    my $class = ref($self);
    Carp::croak ("Tried to set $self to NaN in $class\::_bnan()");
    }

  $IMPORT=1;					# call our import only once
  $self->{_m} = $MBI->bzero();
  $self->{_e} = $MBI->bzero();
  }

sub _binf
  {
  # used by parent class bone() to initialize number to +-inf
  my $self = shift;
  
  if ($_trap_inf)
    {
    require Carp;
    my $class = ref($self);
    Carp::croak ("Tried to set $self to +-inf in $class\::_binf()");
    }

  $IMPORT=1;					# call our import only once
  $self->{_m} = $MBI->bzero();
  $self->{_e} = $MBI->bzero();
  }

sub _bone
  {
  # used by parent class bone() to initialize number to 1
  my $self = shift;
  $IMPORT=1;					# call our import only once
  $self->{_m} = $MBI->bone();
  $self->{_e} = $MBI->bzero();
  }

sub _bzero
  {
  # used by parent class bone() to initialize number to 0
  my $self = shift;
  $IMPORT=1;					# call our import only once
  $self->{_m} = $MBI->bzero();
  $self->{_e} = $MBI->bone();
  }

sub isa
  {
  my ($self,$class) = @_;
  return if $class =~ /^Math::BigInt/;		# we aren't one of these
  UNIVERSAL::isa($self,$class);
  }

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
# string conversation

sub bstr 
  {
  # (ref to BFLOAT or num_str ) return num_str
  # Convert number from internal format to (non-scientific) string format.
  # internal format is always normalized (no leading zeros, "-0" => "+0")
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  if ($x->{sign} !~ /^[+-]$/)
    {
    return $x->{sign} unless $x->{sign} eq '+inf';      # -inf, NaN
    return 'inf';                                       # +inf
    }

  my $es = '0'; my $len = 1; my $cad = 0; my $dot = '.';

  # $x is zero?
  my $not_zero = !($x->{sign} eq '+' && $x->{_m}->is_zero());
  if ($not_zero)
    {
    $es = $x->{_m}->bstr();
    $len = CORE::length($es);
    my $e = $x->{_e}->numify();	
    if ($e < 0)
      {
      $dot = '';
      # if _e is bigger than a scalar, the following will blow your memory
      if ($e <= -$len)
        {
        #print "style: 0.xxxx\n";
        my $r = abs($e) - $len;
        $es = '0.'. ('0' x $r) . $es; $cad = -($len+$r);
        }
      else
        {
        #print "insert '.' at $e in '$es'\n";
        substr($es,$e,0) = '.'; $cad = $x->{_e};
        }
      }
    elsif ($e > 0)
      {
      # expand with zeros
      $es .= '0' x $e; $len += $e; $cad = 0;
      }
    } # if not zero
  $es = '-'.$es if $x->{sign} eq '-';
  # if set accuracy or precision, pad with zeros on the right side
  if ((defined $x->{_a}) && ($not_zero))
    {
    # 123400 => 6, 0.1234 => 4, 0.001234 => 4
    my $zeros = $x->{_a} - $cad;		# cad == 0 => 12340
    $zeros = $x->{_a} - $len if $cad != $len;
    $es .= $dot.'0' x $zeros if $zeros > 0;
    }
  elsif ((($x->{_p} || 0) < 0))
    {
    # 123400 => 6, 0.1234 => 4, 0.001234 => 6
    my $zeros = -$x->{_p} + $cad;
    $es .= $dot.'0' x $zeros if $zeros > 0;
    }
  $es;
  }

sub bsstr
  {
  # (ref to BFLOAT or num_str ) return num_str
  # Convert number from internal format to scientific string format.
  # internal format is always normalized (no leading zeros, "-0E0" => "+0E0")
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  if ($x->{sign} !~ /^[+-]$/)
    {
    return $x->{sign} unless $x->{sign} eq '+inf';      # -inf, NaN
    return 'inf';                                       # +inf
    }
  # do $esign, because we need '1e+1', since $x->{_e}->bstr() misses the +
  my $esign = $x->{_e}->{sign}; $esign = '' if $esign eq '-';
  my $sep = 'e'.$esign;
  my $sign = $x->{sign}; $sign = '' if $sign eq '+';
  $sign . $x->{_m}->bstr() . $sep . $x->{_e}->bstr();
  }
    
sub numify 
  {
  # Make a number from a BigFloat object
  # simple return a string and let Perl's atoi()/atof() handle the rest
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);
  $x->bsstr(); 
  }

##############################################################################
# public stuff (usually prefixed with "b")

# tels 2001-08-04 
# XXX TODO this must be overwritten and return NaN for non-integer values
# band(), bior(), bxor(), too
#sub bnot
#  {
#  $class->SUPER::bnot($class,@_);
#  }

sub bcmp 
  {
  # Compares 2 values.  Returns one of undef, <0, =0, >0. (suitable for sort)

  # set up parameters
  my ($self,$x,$y) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y) = objectify(2,@_);
    }

  return $upgrade->bcmp($x,$y) if defined $upgrade &&
    ((!$x->isa($self)) || (!$y->isa($self)));

  if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/))
    {
    # handle +-inf and NaN
    return undef if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));
    return 0 if ($x->{sign} eq $y->{sign}) && ($x->{sign} =~ /^[+-]inf$/);
    return +1 if $x->{sign} eq '+inf';
    return -1 if $x->{sign} eq '-inf';
    return -1 if $y->{sign} eq '+inf';
    return +1;
    }

  # check sign for speed first
  return 1 if $x->{sign} eq '+' && $y->{sign} eq '-';	# does also 0 <=> -y
  return -1 if $x->{sign} eq '-' && $y->{sign} eq '+';	# does also -x <=> 0

  # shortcut 
  my $xz = $x->is_zero();
  my $yz = $y->is_zero();
  return 0 if $xz && $yz;				# 0 <=> 0
  return -1 if $xz && $y->{sign} eq '+';		# 0 <=> +y
  return 1 if $yz && $x->{sign} eq '+';			# +x <=> 0

  # adjust so that exponents are equal
  my $lxm = $x->{_m}->length();
  my $lym = $y->{_m}->length();
  # the numify somewhat limits our length, but makes it much faster
  my $lx = $lxm + $x->{_e}->numify();
  my $ly = $lym + $y->{_e}->numify();
  my $l = $lx - $ly; $l = -$l if $x->{sign} eq '-';
  return $l <=> 0 if $l != 0;
  
  # lengths (corrected by exponent) are equal
  # so make mantissa equal length by padding with zero (shift left)
  my $diff = $lxm - $lym;
  my $xm = $x->{_m};		# not yet copy it
  my $ym = $y->{_m};
  if ($diff > 0)
    {
    $ym = $y->{_m}->copy()->blsft($diff,10);
    }
  elsif ($diff < 0)
    {
    $xm = $x->{_m}->copy()->blsft(-$diff,10);
    }
  my $rc = $xm->bacmp($ym);
  $rc = -$rc if $x->{sign} eq '-';		# -124 < -123
  $rc <=> 0;
  }

sub bacmp 
  {
  # Compares 2 values, ignoring their signs. 
  # Returns one of undef, <0, =0, >0. (suitable for sort)
  
  # set up parameters
  my ($self,$x,$y) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y) = objectify(2,@_);
    }

  return $upgrade->bacmp($x,$y) if defined $upgrade &&
    ((!$x->isa($self)) || (!$y->isa($self)));

  # handle +-inf and NaN's
  if ($x->{sign} !~ /^[+-]$/ || $y->{sign} !~ /^[+-]$/)
    {
    return undef if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));
    return 0 if ($x->is_inf() && $y->is_inf());
    return 1 if ($x->is_inf() && !$y->is_inf());
    return -1;
    }

  # shortcut 
  my $xz = $x->is_zero();
  my $yz = $y->is_zero();
  return 0 if $xz && $yz;				# 0 <=> 0
  return -1 if $xz && !$yz;				# 0 <=> +y
  return 1 if $yz && !$xz;				# +x <=> 0

  # adjust so that exponents are equal
  my $lxm = $x->{_m}->length();
  my $lym = $y->{_m}->length();
  # the numify somewhat limits our length, but makes it much faster
  my $lx = $lxm + $x->{_e}->numify();
  my $ly = $lym + $y->{_e}->numify();
  my $l = $lx - $ly;
  return $l <=> 0 if $l != 0;
  
  # lengths (corrected by exponent) are equal
  # so make mantissa equal-length by padding with zero (shift left)
  my $diff = $lxm - $lym;
  my $xm = $x->{_m};		# not yet copy it
  my $ym = $y->{_m};
  if ($diff > 0)
    {
    $ym = $y->{_m}->copy()->blsft($diff,10);
    }
  elsif ($diff < 0)
    {
    $xm = $x->{_m}->copy()->blsft(-$diff,10);
    }
  $xm->bacmp($ym) <=> 0;
  }

sub badd 
  {
  # add second arg (BFLOAT or string) to first (BFLOAT) (modifies first)
  # return result as BFLOAT

  # set up parameters
  my ($self,$x,$y,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$a,$p,$r) = objectify(2,@_);
    }

  # inf and NaN handling
  if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/))
    {
    # NaN first
    return $x->bnan() if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));
    # inf handling
    if (($x->{sign} =~ /^[+-]inf$/) && ($y->{sign} =~ /^[+-]inf$/))
      {
      # +inf++inf or -inf+-inf => same, rest is NaN
      return $x if $x->{sign} eq $y->{sign};
      return $x->bnan();
      }
    # +-inf + something => +inf; something +-inf => +-inf
    $x->{sign} = $y->{sign}, return $x if $y->{sign} =~ /^[+-]inf$/;
    return $x;
    }

  return $upgrade->badd($x,$y,$a,$p,$r) if defined $upgrade &&
   ((!$x->isa($self)) || (!$y->isa($self)));

  # speed: no add for 0+y or x+0
  return $x->bround($a,$p,$r) if $y->is_zero();		# x+0
  if ($x->is_zero())					# 0+y
    {
    # make copy, clobbering up x (modify in place!)
    $x->{_e} = $y->{_e}->copy();
    $x->{_m} = $y->{_m}->copy();
    $x->{sign} = $y->{sign} || $nan;
    return $x->round($a,$p,$r,$y);
    }
 
  # take lower of the two e's and adapt m1 to it to match m2
  my $e = $y->{_e};
  $e = $MBI->bzero() if !defined $e;		# if no BFLOAT ?
  $e = $e->copy();				# make copy (didn't do it yet)
  $e->bsub($x->{_e});				# Ye - Xe
  my $add = $y->{_m}->copy();
  if ($e->{sign} eq '-')			# < 0
    {
    $x->{_e} += $e;				# need the sign of e
    $x->{_m}->blsft($e->babs(),10);		# destroys copy of _e
    }
  elsif (!$e->is_zero())			# > 0
    {
    $add->blsft($e,10);
    }
  # else: both e are the same, so just leave them
  $x->{_m}->{sign} = $x->{sign}; 		# fiddle with signs
  $add->{sign} = $y->{sign};
  $x->{_m} += $add; 				# finally do add/sub
  $x->{sign} = $x->{_m}->{sign}; 		# re-adjust signs
  $x->{_m}->{sign} = '+';			# mantissa always positiv
  # delete trailing zeros, then round
  $x->bnorm()->round($a,$p,$r,$y);
  }

sub bsub 
  {
  # (BigFloat or num_str, BigFloat or num_str) return BigFloat
  # subtract second arg from first, modify first

  # set up parameters
  my ($self,$x,$y,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$a,$p,$r) = objectify(2,@_);
    }

  if ($y->is_zero())		# still round for not adding zero
    {
    return $x->round($a,$p,$r);
    }
 
  # $x - $y = -$x + $y 
  $y->{sign} =~ tr/+-/-+/;	# does nothing for NaN
  $x->badd($y,$a,$p,$r);	# badd does not leave internal zeros
  $y->{sign} =~ tr/+-/-+/;	# refix $y (does nothing for NaN)
  $x;				# already rounded by badd()
  }

sub binc
  {
  # increment arg by one
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  if ($x->{_e}->sign() eq '-')
    {
    return $x->badd($self->bone(),@r);	#  digits after dot
    }

  if (!$x->{_e}->is_zero())			# _e == 0 for NaN, inf, -inf
    {
    # 1e2 => 100, so after the shift below _m has a '0' as last digit
    $x->{_m}->blsft($x->{_e},10);		# 1e2 => 100
    $x->{_e}->bzero();				# normalize
    # we know that the last digit of $x will be '1' or '9', depending on the
    # sign
    }
  # now $x->{_e} == 0
  if ($x->{sign} eq '+')
    {
    $x->{_m}->binc();
    return $x->bnorm()->bround(@r);
    }
  elsif ($x->{sign} eq '-')
    {
    $x->{_m}->bdec();
    $x->{sign} = '+' if $x->{_m}->is_zero(); # -1 +1 => -0 => +0
    return $x->bnorm()->bround(@r);
    }
  # inf, nan handling etc
  $x->badd($self->bone(),@r);			# badd() does round 
  }

sub bdec
  {
  # decrement arg by one
  my ($self,$x,@r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  if ($x->{_e}->sign() eq '-')
    {
    return $x->badd($self->bone('-'),@r);	#  digits after dot
    }

  if (!$x->{_e}->is_zero())
    {
    $x->{_m}->blsft($x->{_e},10);		# 1e2 => 100
    $x->{_e}->bzero();
    }
  # now $x->{_e} == 0
  my $zero = $x->is_zero();
  # <= 0
  if (($x->{sign} eq '-') || $zero)
    {
    $x->{_m}->binc();
    $x->{sign} = '-' if $zero;			# 0 => 1 => -1
    $x->{sign} = '+' if $x->{_m}->is_zero();	# -1 +1 => -0 => +0
    return $x->bnorm()->round(@r);
    }
  # > 0
  elsif ($x->{sign} eq '+')
    {
    $x->{_m}->bdec();
    return $x->bnorm()->round(@r);
    }
  # inf, nan handling etc
  $x->badd($self->bone('-'),@r);		# does round 
  } 

sub DEBUG () { 0; }

sub blog
  {
  my ($self,$x,$base,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  # $base > 0, $base != 1; if $base == undef default to $base == e
  # $x >= 0

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my ($scale,@params);
  ($x,@params) = $x->_find_round_parameters($a,$p,$r);

  # also takes care of the "error in _find_round_parameters?" case
  return $x->bnan() if $x->{sign} ne '+' || $x->is_zero();

  # no rounding at all, so must use fallback
  if (scalar @params == 0)
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $params[1] = undef;			# P = undef
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r;			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[0] || $params[1]) + 4;	# take whatever is defined
    }

  return $x->bzero(@params) if $x->is_one();
  # base not defined => base == Euler's constant e
  if (defined $base)
    {
    # make object, since we don't feed it through objectify() to still get the
    # case of $base == undef
    $base = $self->new($base) unless ref($base);
    # $base > 0; $base != 1
    return $x->bnan() if $base->is_zero() || $base->is_one() ||
      $base->{sign} ne '+';
    # if $x == $base, we know the result must be 1.0
    return $x->bone('+',@params) if $x->bcmp($base) == 0;
    }

  # when user set globals, they would interfere with our calculation, so
  # disable them and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;
  local $Math::BigFloat::downgrade = undef;

  # upgrade $x if $x is not a BigFloat (handle BigInt input)
  if (!$x->isa('Math::BigFloat'))
    {
    $x = Math::BigFloat->new($x);
    $self = ref($x);
    }
  
  my $done = 0;

  # If the base is defined and an integer, try to calculate integer result
  # first. This is very fast, and in case the real result was found, we can
  # stop right here.
  if (defined $base && $base->is_int() && $x->is_int())
    {
    my $int = $x->{_m}->copy();
    $int->blsft($x->{_e},10) unless $x->{_e}->is_zero();
    $int->blog($base->as_number());
    # if ($exact)
    if ($base->copy()->bpow($int) == $x)
      {
      # found result, return it
      $x->{_m} = $int;
      $x->{_e} = $MBI->bzero();
      $x->bnorm();
      $done = 1;
      }
    }

  if ($done == 0)
    {
    # first calculate the log to base e (using reduction by 10 (and probably 2))
    $self->_log_10($x,$scale);
 
    # and if a different base was requested, convert it
    if (defined $base)
      {
      $base = Math::BigFloat->new($base) unless $base->isa('Math::BigFloat');
      # not ln, but some other base (don't modify $base)
      $x->bdiv( $base->copy()->blog(undef,$scale), $scale );
      }
    }
 
  # shortcut to not run through _find_round_parameters again
  if (defined $params[0])
    {
    $x->bround($params[0],$params[2]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[1],$params[2]);		# then round accordingly
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    $x->{_a} = undef; $x->{_p} = undef;
    }
  # restore globals
  $$abr = $ab; $$pbr = $pb;

  $x;
  }

sub _log
  {
  # internal log function to calculate ln() based on Taylor series.
  # Modifies $x in place.
  my ($self,$x,$scale) = @_;

  # in case of $x == 1, result is 0
  return $x->bzero() if $x->is_one();

  # http://www.efunda.com/math/taylor_series/logarithmic.cfm?search_string=log

  # u = x-1, v = x+1
  #              _                               _
  # Taylor:     |    u    1   u^3   1   u^5       |
  # ln (x)  = 2 |   --- + - * --- + - * --- + ... |  x > 0
  #             |_   v    3   v^3   5   v^5      _|

  # This takes much more steps to calculate the result and is thus not used
  # u = x-1
  #              _                               _
  # Taylor:     |    u    1   u^2   1   u^3       |
  # ln (x)  = 2 |   --- + - * --- + - * --- + ... |  x > 1/2
  #             |_   x    2   x^2   3   x^3      _|

  my ($limit,$v,$u,$below,$factor,$two,$next,$over,$f);

  $v = $x->copy(); $v->binc();		# v = x+1
  $x->bdec(); $u = $x->copy();		# u = x-1; x = x-1
  $x->bdiv($v,$scale);			# first term: u/v
  $below = $v->copy();
  $over = $u->copy();
  $u *= $u; $v *= $v;				# u^2, v^2
  $below->bmul($v);				# u^3, v^3
  $over->bmul($u);
  $factor = $self->new(3); $f = $self->new(2);

  my $steps = 0 if DEBUG;  
  $limit = $self->new("1E-". ($scale-1));
  while (3 < 5)
    {
    # we calculate the next term, and add it to the last
    # when the next term is below our limit, it won't affect the outcome
    # anymore, so we stop

    # calculating the next term simple from over/below will result in quite
    # a time hog if the input has many digits, since over and below will
    # accumulate more and more digits, and the result will also have many
    # digits, but in the end it is rounded to $scale digits anyway. So if we
    # round $over and $below first, we save a lot of time for the division
    # (not with log(1.2345), but try log (123**123) to see what I mean. This
    # can introduce a rounding error if the division result would be f.i.
    # 0.1234500000001 and we round it to 5 digits it would become 0.12346, but
    # if we truncated $over and $below we might get 0.12345. Does this matter
    # for the end result? So we give $over and $below 4 more digits to be
    # on the safe side (unscientific error handling as usual... :+D
    
    $next = $over->copy->bround($scale+4)->bdiv(
      $below->copy->bmul($factor)->bround($scale+4), 
      $scale);

## old version:    
##    $next = $over->copy()->bdiv($below->copy()->bmul($factor),$scale);

    last if $next->bacmp($limit) <= 0;

    delete $next->{_a}; delete $next->{_p};
    $x->badd($next);
    #print "step  $x\n  ($next - $limit = ",$next - $limit,")\n";
    # calculate things for the next term
    $over *= $u; $below *= $v; $factor->badd($f);
    if (DEBUG)
      {
      $steps++; print "step $steps = $x\n" if $steps % 10 == 0;
      }
    }
  $x->bmul($f);					# $x *= 2
  print "took $steps steps\n" if DEBUG;
  }

sub _log_10
  {
  # Internal log function based on reducing input to the range of 0.1 .. 9.99
  # and then "correcting" the result to the proper one. Modifies $x in place.
  my ($self,$x,$scale) = @_;

  # taking blog() from numbers greater than 10 takes a *very long* time, so we
  # break the computation down into parts based on the observation that:
  #  blog(x*y) = blog(x) + blog(y)
  # We set $y here to multiples of 10 so that $x is below 1 (the smaller $x is
  # the faster it get's, especially because 2*$x takes about 10 times as long,
  # so by dividing $x by 10 we make it at least factor 100 faster...)

  # The same observation is valid for numbers smaller than 0.1 (e.g. computing
  # log(1) is fastest, and the farther away we get from 1, the longer it takes)
  # so we also 'break' this down by multiplying $x with 10 and subtract the
  # log(10) afterwards to get the correct result.

  # calculate nr of digits before dot
  my $dbd = $x->{_m}->length() + $x->{_e}->numify();

  # more than one digit (e.g. at least 10), but *not* exactly 10 to avoid
  # infinite recursion

  my $calc = 1;					# do some calculation?

  # disable the shortcut for 10, since we need log(10) and this would recurse
  # infinitely deep
  if ($x->{_e}->is_one() && $x->{_m}->is_one())
    {
    $dbd = 0;					# disable shortcut
    # we can use the cached value in these cases
    if ($scale <= $LOG_10_A)
      {
      $x->bzero(); $x->badd($LOG_10);
      $calc = 0; 				# no need to calc, but round
      }
    }
  else
    {
    # disable the shortcut for 2, since we maybe have it cached
    if ($x->{_e}->is_zero() && $x->{_m}->bcmp(2) == 0)
      {
      $dbd = 0;					# disable shortcut
      # we can use the cached value in these cases
      if ($scale <= $LOG_2_A)
        {
        $x->bzero(); $x->badd($LOG_2);
        $calc = 0; 				# no need to calc, but round
        }
      }
    }

  # if $x = 0.1, we know the result must be 0-log(10)
  if ($calc != 0 && $x->{_e}->is_one('-') && $x->{_m}->is_one())
    {
    $dbd = 0;					# disable shortcut
    # we can use the cached value in these cases
    if ($scale <= $LOG_10_A)
      {
      $x->bzero(); $x->bsub($LOG_10);
      $calc = 0; 				# no need to calc, but round
      }
    }

  return if $calc == 0;				# already have the result

  # default: these correction factors are undef and thus not used
  my $l_10;				# value of ln(10) to A of $scale
  my $l_2;				# value of ln(2) to A of $scale

  # $x == 2 => 1, $x == 13 => 2, $x == 0.1 => 0, $x == 0.01 => -1
  # so don't do this shortcut for 1 or 0
  if (($dbd > 1) || ($dbd < 0))
    {
    # convert our cached value to an object if not already (avoid doing this
    # at import() time, since not everybody needs this)
    $LOG_10 = $self->new($LOG_10,undef,undef) unless ref $LOG_10;

    #print "x = $x, dbd = $dbd, calc = $calc\n";
    # got more than one digit before the dot, or more than one zero after the
    # dot, so do:
    #  log(123)    == log(1.23) + log(10) * 2
    #  log(0.0123) == log(1.23) - log(10) * 2
  
    if ($scale <= $LOG_10_A)
      {
      # use cached value
      #print "using cached value for l_10\n";
      $l_10 = $LOG_10->copy();		# copy for mul
      }
    else
      {
      # else: slower, compute it (but don't cache it, because it could be big)
      # also disable downgrade for this code path
      local $Math::BigFloat::downgrade = undef;
      #print "l_10 = $l_10 (self = $self', 
      #  ", ref(l_10) = ",ref($l_10)," scale $scale)\n";
      #print "calculating value for l_10, scale $scale\n";
      $l_10 = $self->new(10)->blog(undef,$scale);	# scale+4, actually
      }
    $dbd-- if ($dbd > 1); 		# 20 => dbd=2, so make it dbd=1	
    # make object
    $dbd = $self->new($dbd);
    #print "dbd $dbd\n";  
    $l_10->bmul($dbd);			# log(10) * (digits_before_dot-1)
    #print "l_10 = $l_10\n";
    #print "x = $x";
    $x->{_e}->bsub($dbd);		# 123 => 1.23
    #print " => $x\n";
    #print "calculating log($x) with scale=$scale\n";
 
    }

  # Now: 0.1 <= $x < 10 (and possible correction in l_10)

  ### Since $x in the range 0.5 .. 1.5 is MUCH faster, we do a repeated div
  ### or mul by 2 (maximum times 3, since x < 10 and x > 0.1)

  my $half = $self->new('0.5');
  my $twos = 0;				# default: none (0 times)	
  my $two = $self->new(2);
  while ($x->bacmp($half) <= 0)
    {
    $twos--; $x->bmul($two);
    }
  while ($x->bacmp($two) >= 0)
    {
    $twos++; $x->bdiv($two,$scale+4);		# keep all digits
    }
  #print "$twos\n";
  # $twos > 0 => did mul 2, < 0 => did div 2 (never both)
  # calculate correction factor based on ln(2)
  if ($twos != 0)
    {
    $LOG_2 = $self->new($LOG_2,undef,undef) unless ref $LOG_2;
    if ($scale <= $LOG_2_A)
      {
      # use cached value
      #print "using cached value for l_10\n";
      $l_2 = $LOG_2->copy();			# copy for mul
      }
    else
      {
      # else: slower, compute it (but don't cache it, because it could be big)
      # also disable downgrade for this code path
      local $Math::BigFloat::downgrade = undef;
      #print "calculating value for l_2, scale $scale\n";
      $l_2 = $two->blog(undef,$scale);	# scale+4, actually
      }
    $l_2->bmul($twos);		# * -2 => subtract, * 2 => add
    }
  
  $self->_log($x,$scale);			# need to do the "normal" way
  $x->badd($l_10) if defined $l_10; 		# correct it by ln(10)
  $x->badd($l_2) if defined $l_2;		# and maybe by ln(2)
  # all done, $x contains now the result
  }

sub blcm 
  { 
  # (BFLOAT or num_str, BFLOAT or num_str) return BFLOAT
  # does not modify arguments, but returns new object
  # Lowest Common Multiplicator

  my ($self,@arg) = objectify(0,@_);
  my $x = $self->new(shift @arg);
  while (@arg) { $x = _lcm($x,shift @arg); } 
  $x;
  }

sub bgcd 
  { 
  # (BFLOAT or num_str, BFLOAT or num_str) return BINT
  # does not modify arguments, but returns new object
  # GCD -- Euclids algorithm Knuth Vol 2 pg 296
   
  my ($self,@arg) = objectify(0,@_);
  my $x = $self->new(shift @arg);
  while (@arg) { $x = _gcd($x,shift @arg); } 
  $x;
  }

###############################################################################
# is_foo methods (is_negative, is_positive are inherited from BigInt)

sub _is_zero_or_one
  {
  # internal, return true if BigInt arg is zero or one, saving the
  # two calls to is_zero() and is_one() 
  my $x = $_[0];

  $x->{sign} eq '+' && ($x->is_zero() || $x->is_one());
  }

sub is_int
  {
  # return true if arg (BFLOAT or num_str) is an integer
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return 1 if ($x->{sign} =~ /^[+-]$/) &&	# NaN and +-inf aren't
    $x->{_e}->{sign} eq '+';			# 1e-1 => no integer
  0;
  }

sub is_zero
  {
  # return true if arg (BFLOAT or num_str) is zero
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return 1 if $x->{sign} eq '+' && $x->{_m}->is_zero();
  0;
  }

sub is_one
  {
  # return true if arg (BFLOAT or num_str) is +1 or -1 if signis given
  my ($self,$x,$sign) = ref($_[0]) ? (undef,@_) : objectify(1,@_);

  $sign = '+' if !defined $sign || $sign ne '-';
  return 1
   if ($x->{sign} eq $sign && $x->{_e}->is_zero() && $x->{_m}->is_one()); 
  0;
  }

sub is_odd
  {
  # return true if arg (BFLOAT or num_str) is odd or false if even
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);
  
  return 1 if ($x->{sign} =~ /^[+-]$/) &&		# NaN & +-inf aren't
    ($x->{_e}->is_zero() && $x->{_m}->is_odd()); 
  0;
  }

sub is_even
  {
  # return true if arg (BINT or num_str) is even or false if odd
  my ($self,$x) = ref($_[0]) ? (undef,$_[0]) : objectify(1,@_);

  return 0 if $x->{sign} !~ /^[+-]$/;			# NaN & +-inf aren't
  return 1 if ($x->{_e}->{sign} eq '+' 			# 123.45 is never
     && $x->{_m}->is_even()); 				# but 1200 is
  0;
  }

sub bmul 
  { 
  # multiply two numbers -- stolen from Knuth Vol 2 pg 233
  # (BINT or num_str, BINT or num_str) return BINT
  
  # set up parameters
  my ($self,$x,$y,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$a,$p,$r) = objectify(2,@_);
    }

  return $x->bnan() if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));

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
  # handle result = 0
  return $x->bzero() if $x->is_zero() || $y->is_zero();
  
  return $upgrade->bmul($x,$y,$a,$p,$r) if defined $upgrade &&
   ((!$x->isa($self)) || (!$y->isa($self)));

  # aEb * cEd = (a*c)E(b+d)
  $x->{_m}->bmul($y->{_m});
  $x->{_e}->badd($y->{_e});
  # adjust sign:
  $x->{sign} = $x->{sign} ne $y->{sign} ? '-' : '+';
  return $x->bnorm()->round($a,$p,$r,$y);
  }

sub bdiv 
  {
  # (dividend: BFLOAT or num_str, divisor: BFLOAT or num_str) return 
  # (BFLOAT,BFLOAT) (quo,rem) or BFLOAT (only rem)

  # set up parameters
  my ($self,$x,$y,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$a,$p,$r) = objectify(2,@_);
    }

  return $self->_div_inf($x,$y)
   if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/) || $y->is_zero());

  # x== 0 # also: or y == 1 or y == -1
  return wantarray ? ($x,$self->bzero()) : $x if $x->is_zero();

  # upgrade ?
  return $upgrade->bdiv($upgrade->new($x),$y,$a,$p,$r) if defined $upgrade;

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my (@params,$scale);
  ($x,@params) = $x->_find_round_parameters($a,$p,$r,$y);

  return $x if $x->is_nan();		# error in _find_round_parameters?

  # no rounding at all, so must use fallback
  if (scalar @params == 0)
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r;			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[0] || $params[1]) + 4;	# take whatever is defined
    }
  my $lx = $x->{_m}->length(); my $ly = $y->{_m}->length();
  $scale = $lx if $lx > $scale;
  $scale = $ly if $ly > $scale;
  my $diff = $ly - $lx;
  $scale += $diff if $diff > 0;		# if lx << ly, but not if ly << lx!
    
  # make copy of $x in case of list context for later reminder calculation
  my $rem;
  if (wantarray && !$y->is_one())
    {
    $rem = $x->copy();
    }

  $x->{sign} = $x->{sign} ne $y->sign() ? '-' : '+'; 

  # check for / +-1 ( +/- 1E0)
  if (!$y->is_one())
    {
    # promote BigInts and it's subclasses (except when already a BigFloat)
    $y = $self->new($y) unless $y->isa('Math::BigFloat'); 

    # need to disable $upgrade in BigInt, to avoid deep recursion
    local $Math::BigInt::upgrade = undef; 	# should be parent class vs MBI

    # calculate the result to $scale digits and then round it
    # a * 10 ** b / c * 10 ** d => a/c * 10 ** (b-d)
    $x->{_m}->blsft($scale,10);
    $x->{_m}->bdiv( $y->{_m} );	# a/c
    $x->{_e}->bsub( $y->{_e} );	# b-d
    $x->{_e}->bsub($scale);	# correct for 10**scale
    $x->bnorm();		# remove trailing 0's
    }

  # shortcut to not run through _find_round_parameters again
  if (defined $params[0])
    {
    $x->{_a} = undef; 				# clear before round
    $x->bround($params[0],$params[2]);		# then round accordingly
    }
  else
    {
    $x->{_p} = undef; 				# clear before round
    $x->bfround($params[1],$params[2]);		# then round accordingly
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    $x->{_a} = undef; $x->{_p} = undef;
    }
  
  if (wantarray)
    {
    if (!$y->is_one())
      {
      $rem->bmod($y,@params);			# copy already done
      }
    else
      {
      $rem = $self->bzero();
      }
    if ($fallback)
      {
      # clear a/p after round, since user did not request it
      $rem->{_a} = undef; $rem->{_p} = undef;
      }
    return ($x,$rem);
    }
  $x;
  }

sub bmod 
  {
  # (dividend: BFLOAT or num_str, divisor: BFLOAT or num_str) return reminder 

  # set up parameters
  my ($self,$x,$y,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$a,$p,$r) = objectify(2,@_);
    }

  if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/))
    {
    my ($d,$re) = $self->SUPER::_div_inf($x,$y);
    $x->{sign} = $re->{sign};
    $x->{_e} = $re->{_e};
    $x->{_m} = $re->{_m};
    return $x->round($a,$p,$r,$y);
    } 
  return $x->bnan() if $x->is_zero() && $y->is_zero();
  return $x if $y->is_zero();
  return $x->bnan() if $x->is_nan() || $y->is_nan();
  return $x->bzero() if $y->is_one() || $x->is_zero();

  # inf handling is missing here
 
  my $cmp = $x->bacmp($y);			# equal or $x < $y?
  return $x->bzero($a,$p) if $cmp == 0;		# $x == $y => result 0

  # only $y of the operands negative? 
  my $neg = 0; $neg = 1 if $x->{sign} ne $y->{sign};

  $x->{sign} = $y->{sign};				# calc sign first
  return $x->round($a,$p,$r) if $cmp < 0 && $neg == 0;	# $x < $y => result $x
  
  my $ym = $y->{_m}->copy();
  
  # 2e1 => 20
  $ym->blsft($y->{_e},10) if $y->{_e}->{sign} eq '+' && !$y->{_e}->is_zero();
 
  # if $y has digits after dot
  my $shifty = 0;			# correct _e of $x by this
  if ($y->{_e}->{sign} eq '-')		# has digits after dot
    {
    # 123 % 2.5 => 1230 % 25 => 5 => 0.5
    $shifty = $y->{_e}->copy()->babs();	# no more digits after dot
    $x->blsft($shifty,10);		# 123 => 1230, $y->{_m} is already 25
    }
  # $ym is now mantissa of $y based on exponent 0

  my $shiftx = 0;			# correct _e of $x by this
  if ($x->{_e}->{sign} eq '-')		# has digits after dot
    {
    # 123.4 % 20 => 1234 % 200
    $shiftx = $x->{_e}->copy()->babs();	# no more digits after dot
    $ym->blsft($shiftx,10);
    }
  # 123e1 % 20 => 1230 % 20
  if ($x->{_e}->{sign} eq '+' && !$x->{_e}->is_zero())
    {
    $x->{_m}->blsft($x->{_e},10);
    }
  $x->{_e} = $MBI->bzero() unless $x->{_e}->is_zero();
  
  $x->{_e}->bsub($shiftx) if $shiftx != 0;
  $x->{_e}->bsub($shifty) if $shifty != 0;
  
  # now mantissas are equalized, exponent of $x is adjusted, so calc result

  $x->{_m}->bmod($ym);

  $x->{sign} = '+' if $x->{_m}->is_zero();		# fix sign for -0
  $x->bnorm();

  if ($neg != 0)	# one of them negative => correct in place
    {
    my $r = $y - $x;
    $x->{_m} = $r->{_m};
    $x->{_e} = $r->{_e};
    $x->{sign} = '+' if $x->{_m}->is_zero();		# fix sign for -0
    $x->bnorm();
    }

  $x->round($a,$p,$r,$y);	# round and return
  }

sub broot
  {
  # calculate $y'th root of $x
  
  # set up parameters
  my ($self,$x,$y,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$a,$p,$r) = objectify(2,@_);
    }

  # NaN handling: $x ** 1/0, x or y NaN, or y inf/-inf or y == 0
  return $x->bnan() if $x->{sign} !~ /^\+/ || $y->is_zero() ||
         $y->{sign} !~ /^\+$/;

  return $x if $x->is_zero() || $x->is_one() || $x->is_inf() || $y->is_one();
  
  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my (@params,$scale);
  ($x,@params) = $x->_find_round_parameters($a,$p,$r);

  return $x if $x->is_nan();		# error in _find_round_parameters?

  # no rounding at all, so must use fallback
  if (scalar @params == 0) 
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r;			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[0] || $params[1]) + 4; # take whatever is defined
    }

  # when user set globals, they would interfere with our calculation, so
  # disable them and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;	# should be really parent class vs MBI

  # remember sign and make $x positive, since -4 ** (1/2) => -2
  my $sign = 0; $sign = 1 if $x->is_negative(); $x->babs();

  if ($y->bcmp(2) == 0)		# normal square root
    {
    $x->bsqrt($scale+4);
    }
  elsif ($y->is_one('-'))
    {
    # $x ** -1 => 1/$x
    my $u = $self->bone()->bdiv($x,$scale);
    # copy private parts over
    $x->{_m} = $u->{_m};
    $x->{_e} = $u->{_e};
    }
  else
    {
    # calculate the broot() as integer result first, and if it fits, return
    # it rightaway (but only if $x and $y are integer):

    my $done = 0;				# not yet
    if ($y->is_int() && $x->is_int())
      {
      my $int = $x->{_m}->copy();
      $int->blsft($x->{_e},10) unless $x->{_e}->is_zero();
      $int->broot($y->as_number());
      # if ($exact)
      if ($int->copy()->bpow($y) == $x)
        {
        # found result, return it
        $x->{_m} = $int;
        $x->{_e} = $MBI->bzero();
        $x->bnorm();
        $done = 1;
        }
      }
    if ($done == 0)
      {
      my $u = $self->bone()->bdiv($y,$scale+4);
      delete $u->{_a}; delete $u->{_p};         # otherwise it conflicts
      $x->bpow($u,$scale+4);                    # el cheapo
      }
    }
  $x->bneg() if $sign == 1;
  
  # shortcut to not run through _find_round_parameters again
  if (defined $params[0])
    {
    $x->bround($params[0],$params[2]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[1],$params[2]);		# then round accordingly
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    $x->{_a} = undef; $x->{_p} = undef;
    }
  # restore globals
  $$abr = $ab; $$pbr = $pb;
  $x;
  }

sub bsqrt
  { 
  # calculate square root
  my ($self,$x,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x->bnan() if $x->{sign} !~ /^[+]/;	# NaN, -inf or < 0
  return $x if $x->{sign} eq '+inf';		# sqrt(inf) == inf
  return $x->round($a,$p,$r) if $x->is_zero() || $x->is_one();

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my (@params,$scale);
  ($x,@params) = $x->_find_round_parameters($a,$p,$r);

  return $x if $x->is_nan();		# error in _find_round_parameters?

  # no rounding at all, so must use fallback
  if (scalar @params == 0) 
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r;			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[0] || $params[1]) + 4; # take whatever is defined
    }

  # when user set globals, they would interfere with our calculation, so
  # disable them and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;	# should be really parent class vs MBI

  my $xas = $x->as_number();
  my $gs = $xas->copy()->bsqrt();	# some guess

  if (($x->{_e}->{sign} ne '-')		# guess can't be accurate if there are
					# digits after the dot
   && ($xas->bacmp($gs * $gs) == 0))	# guess hit the nail on the head?
    {
    # exact result
    $x->{_m} = $gs; $x->{_e} = $MBI->bzero(); $x->bnorm();
    # shortcut to not run through _find_round_parameters again
    if (defined $params[0])
      {
      $x->bround($params[0],$params[2]);	# then round accordingly
      }
    else
      {
      $x->bfround($params[1],$params[2]);	# then round accordingly
      }
    if ($fallback)
      {
      # clear a/p after round, since user did not request it
      $x->{_a} = undef; $x->{_p} = undef;
      }
    # re-enable A and P, upgrade is taken care of by "local"
    ${"$self\::accuracy"} = $ab; ${"$self\::precision"} = $pb;
    return $x;
    }
 
  # sqrt(2) = 1.4 because sqrt(2*100) = 1.4*10; so we can increase the accuracy
  # of the result by multipyling the input by 100 and then divide the integer
  # result of sqrt(input) by 10. Rounding afterwards returns the real result.
  # this will transform 123.456 (in $x) into 123456 (in $y1)
  my $y1 = $x->{_m}->copy();
  # We now make sure that $y1 has the same odd or even number of digits than
  # $x had. So when _e of $x is odd, we must shift $y1 by one digit left,
  # because we always must multiply by steps of 100 (sqrt(100) is 10) and not
  # steps of 10. The length of $x does not count, since an even or odd number
  # of digits before the dot is not changed by adding an even number of digits
  # after the dot (the result is still odd or even digits long).
  my $length = $y1->length();
  $y1->bmul(10) if $x->{_e}->is_odd();
  # now calculate how many digits the result of sqrt(y1) would have
  my $digits = int($length / 2);
  # but we need at least $scale digits, so calculate how many are missing
  my $shift = $scale - $digits;
  # that should never happen (we take care of integer guesses above)
  # $shift = 0 if $shift < 0; 
  # multiply in steps of 100, by shifting left two times the "missing" digits
  $y1->blsft($shift*2,10);
  # now take the square root and truncate to integer
  $y1->bsqrt();
  # By "shifting" $y1 right (by creating a negative _e) we calculate the final
  # result, which is than later rounded to the desired scale.

  # calculate how many zeros $x had after the '.' (or before it, depending
  #  on sign of $dat, the result should have half as many:
  my $dat = $length + $x->{_e}->numify();

  if ($dat > 0)
    {
    # no zeros after the dot (e.g. 1.23, 0.49 etc)
    # preserve half as many digits before the dot than the input had 
    # (but round this "up")
    $dat = int(($dat+1)/2);
    }
  else
    {
    $dat = int(($dat)/2);
    }
  $x->{_e}= $MBI->new( $dat - $y1->length() );

  $x->{_m} = $y1;

  # shortcut to not run through _find_round_parameters again
  if (defined $params[0])
    {
    $x->bround($params[0],$params[2]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[1],$params[2]);		# then round accordingly
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    $x->{_a} = undef; $x->{_p} = undef;
    }
  # restore globals
  $$abr = $ab; $$pbr = $pb;
  $x;
  }

sub bfac
  {
  # (BFLOAT or num_str, BFLOAT or num_str) return BFLOAT
  # compute factorial number, modifies first argument

  # set up parameters
  my ($self,$x,@r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  ($self,$x,@r) = objectify(1,@_) if !ref($x);

 return $x if $x->{sign} eq '+inf';	# inf => inf
  return $x->bnan() 
    if (($x->{sign} ne '+') ||		# inf, NaN, <0 etc => NaN
     ($x->{_e}->{sign} ne '+'));	# digits after dot?

  # use BigInt's bfac() for faster calc
  if (! $x->{_e}->is_zero())
    {
    $x->{_m}->blsft($x->{_e},10);	# change 12e1 to 120e0
    $x->{_e}->bzero();
    }
  $x->{_m}->bfac();			# calculate factorial
  $x->bnorm()->round(@r); 		# norm again and round result
  }

sub _pow
  {
  # Calculate a power where $y is a non-integer, like 2 ** 0.5
  my ($x,$y,$a,$p,$r) = @_;
  my $self = ref($x);

  # if $y == 0.5, it is sqrt($x)
  return $x->bsqrt($a,$p,$r,$y) if $y->bcmp('0.5') == 0;

  # Using:
  # a ** x == e ** (x * ln a)

  # u = y * ln x
  #                _                         _
  # Taylor:       |   u    u^2    u^3         |
  # x ** y  = 1 + |  --- + --- + ----- + ...  |
  #               |_  1    1*2   1*2*3       _|

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my ($scale,@params);
  ($x,@params) = $x->_find_round_parameters($a,$p,$r);
    
  return $x if $x->is_nan();		# error in _find_round_parameters?

  # no rounding at all, so must use fallback
  if (scalar @params == 0)
    {
    # simulate old behaviour
    $params[0] = $self->div_scale();	# and round to it as accuracy
    $params[1] = undef;			# disable P
    $scale = $params[0]+4; 		# at least four more for proper round
    $params[2] = $r;			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[0] || $params[1]) + 4; # take whatever is defined
    }

  # when user set globals, they would interfere with our calculation, so
  # disable them and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;
 
  my ($limit,$v,$u,$below,$factor,$next,$over);

  $u = $x->copy()->blog(undef,$scale)->bmul($y);
  $v = $self->bone();				# 1
  $factor = $self->new(2);			# 2
  $x->bone();					# first term: 1

  $below = $v->copy();
  $over = $u->copy();
 
  $limit = $self->new("1E-". ($scale-1));
  #my $steps = 0;
  while (3 < 5)
    {
    # we calculate the next term, and add it to the last
    # when the next term is below our limit, it won't affect the outcome
    # anymore, so we stop
    $next = $over->copy()->bdiv($below,$scale);
    last if $next->bacmp($limit) <= 0;
    $x->badd($next);
    # calculate things for the next term
    $over *= $u; $below *= $factor; $factor->binc();
    #$steps++;
    }
  
  # shortcut to not run through _find_round_parameters again
  if (defined $params[0])
    {
    $x->bround($params[0],$params[2]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[1],$params[2]);		# then round accordingly
    }
  if ($fallback)
    {
    # clear a/p after round, since user did not request it
    $x->{_a} = undef; $x->{_p} = undef;
    }
  # restore globals
  $$abr = $ab; $$pbr = $pb;
  $x;
  }

sub bpow 
  {
  # (BFLOAT or num_str, BFLOAT or num_str) return BFLOAT
  # compute power of two numbers, second arg is used as integer
  # modifies first argument

  # set up parameters
  my ($self,$x,$y,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$a,$p,$r) = objectify(2,@_);
    }

  return $x if $x->{sign} =~ /^[+-]inf$/;
  return $x->bnan() if $x->{sign} eq $nan || $y->{sign} eq $nan;
  return $x->bone() if $y->is_zero();
  return $x         if $x->is_one() || $y->is_one();

  return $x->_pow($y,$a,$p,$r) if !$y->is_int();	# non-integer power

  my $y1 = $y->as_number();		# make bigint
  # if ($x == -1)
  if ($x->{sign} eq '-' && $x->{_m}->is_one() && $x->{_e}->is_zero())
    {
    # if $x == -1 and odd/even y => +1/-1  because +-1 ^ (+-1) => +-1
    return $y1->is_odd() ? $x : $x->babs(1);
    }
  if ($x->is_zero())
    {
    return $x if $y->{sign} eq '+'; 	# 0**y => 0 (if not y <= 0)
    # 0 ** -y => 1 / (0 ** y) => / 0! (1 / 0 => +inf)
    $x->binf();
    }

  # calculate $x->{_m} ** $y and $x->{_e} * $y separately (faster)
  $y1->babs();
  $x->{_m}->bpow($y1);
  $x->{_e}->bmul($y1);
  $x->{sign} = $nan if $x->{_m}->{sign} eq $nan || $x->{_e}->{sign} eq $nan;
  $x->bnorm();
  if ($y->{sign} eq '-')
    {
    # modify $x in place!
    my $z = $x->copy(); $x->bzero()->binc();
    return $x->bdiv($z,$a,$p,$r);	# round in one go (might ignore y's A!)
    }
  $x->round($a,$p,$r,$y);
  }

###############################################################################
# rounding functions

sub bfround
  {
  # precision: round to the $Nth digit left (+$n) or right (-$n) from the '.'
  # $n == 0 means round to integer
  # expects and returns normalized numbers!
  my $x = shift; my $self = ref($x) || $x; $x = $self->new(shift) if !ref($x);

  return $x if $x->modify('bfround');
  
  my ($scale,$mode) = $x->_scale_p($self->precision(),$self->round_mode(),@_);
  return $x if !defined $scale;			# no-op

  # never round a 0, +-inf, NaN
  if ($x->is_zero())
    {
    $x->{_p} = $scale if !defined $x->{_p} || $x->{_p} < $scale; # -3 < -2
    return $x; 
    }
  return $x if $x->{sign} !~ /^[+-]$/;

  # don't round if x already has lower precision
  return $x if (defined $x->{_p} && $x->{_p} < 0 && $scale < $x->{_p});

  $x->{_p} = $scale;			# remember round in any case
  $x->{_a} = undef;			# and clear A
  if ($scale < 0)
    {
    # round right from the '.'

    return $x if $x->{_e}->{sign} eq '+';	# e >= 0 => nothing to round

    $scale = -$scale;				# positive for simplicity
    my $len = $x->{_m}->length();		# length of mantissa

    # the following poses a restriction on _e, but if _e is bigger than a
    # scalar, you got other problems (memory etc) anyway
    my $dad = -($x->{_e}->numify());		# digits after dot
    my $zad = 0;				# zeros after dot
    $zad = $dad - $len if (-$dad < -$len);	# for 0.00..00xxx style
    
    #print "scale $scale dad $dad zad $zad len $len\n";
    # number  bsstr   len zad dad	
    # 0.123   123e-3	3   0 3
    # 0.0123  123e-4	3   1 4
    # 0.001   1e-3      1   2 3
    # 1.23    123e-2	3   0 2
    # 1.2345  12345e-4	5   0 4

    # do not round after/right of the $dad
    return $x if $scale > $dad;			# 0.123, scale >= 3 => exit

    # round to zero if rounding inside the $zad, but not for last zero like:
    # 0.0065, scale -2, round last '0' with following '65' (scale == zad case)
    return $x->bzero() if $scale < $zad;
    if ($scale == $zad)			# for 0.006, scale -3 and trunc
      {
      $scale = -$len;
      }
    else
      {
      # adjust round-point to be inside mantissa
      if ($zad != 0)
        {
	$scale = $scale-$zad;
        }
      else
        {
        my $dbd = $len - $dad; $dbd = 0 if $dbd < 0;	# digits before dot
	$scale = $dbd+$scale;
        }
      }
    }
  else
    {
    # round left from the '.'

    # 123 => 100 means length(123) = 3 - $scale (2) => 1

    my $dbt = $x->{_m}->length(); 
    # digits before dot 
    my $dbd = $dbt + $x->{_e}->numify(); 
    # should be the same, so treat it as this 
    $scale = 1 if $scale == 0; 
    # shortcut if already integer 
    return $x if $scale == 1 && $dbt <= $dbd; 
    # maximum digits before dot 
    ++$dbd;

    if ($scale > $dbd) 
       { 
       # not enough digits before dot, so round to zero 
       return $x->bzero; 
       }
    elsif ( $scale == $dbd )
       { 
       # maximum 
       $scale = -$dbt; 
       } 
    else
       { 
       $scale = $dbd - $scale; 
       }
    }
  # pass sign to bround for rounding modes '+inf' and '-inf'
  $x->{_m}->{sign} = $x->{sign};
  $x->{_m}->bround($scale,$mode);
  $x->{_m}->{sign} = '+';		# fix sign back
  $x->bnorm();
  }

sub bround
  {
  # accuracy: preserve $N digits, and overwrite the rest with 0's
  my $x = shift; my $self = ref($x) || $x; $x = $self->new(shift) if !ref($x);
  
  if (($_[0] || 0) < 0)
    {
    require Carp; Carp::croak ('bround() needs positive accuracy');
    }

  my ($scale,$mode) = $x->_scale_a($self->accuracy(),$self->round_mode(),@_);
  return $x if !defined $scale;				# no-op

  return $x if $x->modify('bround');

  # scale is now either $x->{_a}, $accuracy, or the user parameter
  # test whether $x already has lower accuracy, do nothing in this case 
  # but do round if the accuracy is the same, since a math operation might
  # want to round a number with A=5 to 5 digits afterwards again
  return $x if defined $_[0] && defined $x->{_a} && $x->{_a} < $_[0];

  # scale < 0 makes no sense
  # never round a +-inf, NaN
  return $x if ($scale < 0) ||	$x->{sign} !~ /^[+-]$/;

  # 1: $scale == 0 => keep all digits
  # 2: never round a 0
  # 3: if we should keep more digits than the mantissa has, do nothing
  if ($scale == 0 || $x->is_zero() || $x->{_m}->length() <= $scale)
    {
    $x->{_a} = $scale if !defined $x->{_a} || $x->{_a} > $scale;
    return $x; 
    }

  # pass sign to bround for '+inf' and '-inf' rounding modes
  $x->{_m}->{sign} = $x->{sign};
  $x->{_m}->bround($scale,$mode);	# round mantissa
  $x->{_m}->{sign} = '+';		# fix sign back
  # $x->{_m}->{_a} = undef; $x->{_m}->{_p} = undef;
  $x->{_a} = $scale;			# remember rounding
  $x->{_p} = undef;			# and clear P
  $x->bnorm();				# del trailing zeros gen. by bround()
  }

sub bfloor
  {
  # return integer less or equal then $x
  my ($self,$x,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x if $x->modify('bfloor');
   
  return $x if $x->{sign} !~ /^[+-]$/;	# nan, +inf, -inf

  # if $x has digits after dot
  if ($x->{_e}->{sign} eq '-')
    {
    $x->{_e}->{sign} = '+';			# negate e
    $x->{_m}->brsft($x->{_e},10);		# cut off digits after dot
    $x->{_e}->bzero();				# trunc/norm	
    $x->{_m}->binc() if $x->{sign} eq '-';	# decrement if negative
    }
  $x->round($a,$p,$r);
  }

sub bceil
  {
  # return integer greater or equal then $x
  my ($self,$x,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x if $x->modify('bceil');
  return $x if $x->{sign} !~ /^[+-]$/;	# nan, +inf, -inf

  # if $x has digits after dot
  if ($x->{_e}->{sign} eq '-')
    {
    #$x->{_m}->brsft(-$x->{_e},10);
    #$x->{_e}->bzero();
    #$x++ if $x->{sign} eq '+';

    $x->{_e}->{sign} = '+';			# negate e
    $x->{_m}->brsft($x->{_e},10);		# cut off digits after dot
    $x->{_e}->bzero();				# trunc/norm	
    $x->{_m}->binc() if $x->{sign} eq '+';	# decrement if negative
    }
  $x->round($a,$p,$r);
  }

sub brsft
  {
  # shift right by $y (divide by power of $n)
  
  # set up parameters
  my ($self,$x,$y,$n,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$n,$a,$p,$r) = objectify(2,@_);
    }

  return $x if $x->modify('brsft');
  return $x if $x->{sign} !~ /^[+-]$/;	# nan, +inf, -inf

  $n = 2 if !defined $n; $n = $self->new($n);
  $x->bdiv($n->bpow($y),$a,$p,$r,$y);
  }

sub blsft
  {
  # shift left by $y (multiply by power of $n)
  
  # set up parameters
  my ($self,$x,$y,$n,$a,$p,$r) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y,$n,$a,$p,$r) = objectify(2,@_);
    }

  return $x if $x->modify('blsft');
  return $x if $x->{sign} !~ /^[+-]$/;	# nan, +inf, -inf

  $n = 2 if !defined $n; $n = $self->new($n);
  $x->bmul($n->bpow($y),$a,$p,$r,$y);
  }

###############################################################################

sub DESTROY
  {
  # going through AUTOLOAD for every DESTROY is costly, avoid it by empty sub
  }

sub AUTOLOAD
  {
  # make fxxx and bxxx both work by selectively mapping fxxx() to MBF::bxxx()
  # or falling back to MBI::bxxx()
  my $name = $AUTOLOAD;

  $name =~ s/.*:://;	# split package
  no strict 'refs';
  $class->import() if $IMPORT == 0;
  if (!method_alias($name))
    {
    if (!defined $name)
      {
      # delayed load of Carp and avoid recursion	
      require Carp;
      Carp::croak ("Can't call a method without name");
      }
    if (!method_hand_up($name))
      {
      # delayed load of Carp and avoid recursion	
      require Carp;
      Carp::croak ("Can't call $class\-\>$name, not a valid method");
      }
    # try one level up, but subst. bxxx() for fxxx() since MBI only got bxxx()
    $name =~ s/^f/b/;
    return &{"$MBI"."::$name"}(@_);
    }
  my $bname = $name; $bname =~ s/^f/b/;
  *{$class."::$name"} = \&$bname;
  &$bname;	# uses @_
  }

sub exponent
  {
  # return a copy of the exponent
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  if ($x->{sign} !~ /^[+-]$/)
    {
    my $s = $x->{sign}; $s =~ s/^[+-]//;
    return $self->new($s);	 		# -inf, +inf => +inf
    }
  return $x->{_e}->copy();
  }

sub mantissa
  {
  # return a copy of the mantissa
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);
 
  if ($x->{sign} !~ /^[+-]$/)
    {
    my $s = $x->{sign}; $s =~ s/^[+]//;
    return $self->new($s); 			# -inf, +inf => +inf
    }
  my $m = $x->{_m}->copy();		# faster than going via bstr()
  $m->bneg() if $x->{sign} eq '-';

  $m;
  }

sub parts
  {
  # return a copy of both the exponent and the mantissa
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  if ($x->{sign} !~ /^[+-]$/)
    {
    my $s = $x->{sign}; $s =~ s/^[+]//; my $se = $s; $se =~ s/^[-]//;
    return ($self->new($s),$self->new($se)); # +inf => inf and -inf,+inf => inf
    }
  my $m = $x->{_m}->copy();	# faster than going via bstr()
  $m->bneg() if $x->{sign} eq '-';
  return ($m,$x->{_e}->copy());
  }

##############################################################################
# private stuff (internal use only)

sub import
  {
  my $self = shift;
  my $l = scalar @_;
  my $lib = ''; my @a;
  $IMPORT=1;
  for ( my $i = 0; $i < $l ; $i++)
    {
    if ( $_[$i] eq ':constant' )
      {
      # This causes overlord er load to step in. 'binary' and 'integer'
      # are handled by BigInt.
      overload::constant float => sub { $self->new(shift); }; 
      }
    elsif ($_[$i] eq 'upgrade')
      {
      # this causes upgrading
      $upgrade = $_[$i+1];		# or undef to disable
      $i++;
      }
    elsif ($_[$i] eq 'downgrade')
      {
      # this causes downgrading
      $downgrade = $_[$i+1];		# or undef to disable
      $i++;
      }
    elsif ($_[$i] eq 'lib')
      {
      # alternative library
      $lib = $_[$i+1] || '';		# default Calc
      $i++;
      }
    elsif ($_[$i] eq 'with')
      {
      # alternative class for our private parts()
      $MBI = $_[$i+1] || 'Math::BigInt';	# default Math::BigInt
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
    # MBI not loaded, or with ne "Math::BigInt"
    $lib .= ",$mbilib" if defined $mbilib;
    $lib =~ s/^,//;				# don't leave empty 
    # replacement library can handle lib statement, but also could ignore it
    if ($] < 5.006)
      {
      # Perl < 5.6.0 dies with "out of memory!" when eval() and ':constant' is
      # used in the same script, or eval inside import().
      my @parts = split /::/, $MBI;		# Math::BigInt => Math BigInt
      my $file = pop @parts; $file .= '.pm';	# BigInt => BigInt.pm
      require File::Spec;
      $file = File::Spec->catfile (@parts, $file);
      eval { require "$file"; };
      $MBI->import( lib => $lib, 'objectify' );
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

  # any non :constant stuff is handled by our parent, Exporter
  # even if @_ is empty, to give it a chance
  $self->SUPER::import(@a);      	# for subclasses
  $self->export_to_level(1,$self,@a);	# need this, too
  }

sub bnorm
  {
  # adjust m and e so that m is smallest possible
  # round number according to accuracy and precision settings
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $x if $x->{sign} !~ /^[+-]$/;		# inf, nan etc

  my $zeros = $x->{_m}->_trailing_zeros();	# correct for trailing zeros 
  if ($zeros != 0)
    {
    my $z = $MBI->new($zeros,undef,undef);
    $x->{_m}->brsft($z,10); $x->{_e}->badd($z);
    }
  else
    {
    # $x can only be 0Ey if there are no trailing zeros ('0' has 0 trailing
    # zeros). So, for something like 0Ey, set y to 1, and -0 => +0
    $x->{sign} = '+', $x->{_e}->bone() if $x->{_m}->is_zero();
    }

  # this is to prevent automatically rounding when MBI's globals are set
  $x->{_m}->{_f} = MB_NEVER_ROUND;
  $x->{_e}->{_f} = MB_NEVER_ROUND;
  # 'forget' that mantissa was rounded via MBI::bround() in MBF's bfround()
  $x->{_m}->{_a} = undef; $x->{_e}->{_a} = undef;
  $x->{_m}->{_p} = undef; $x->{_e}->{_p} = undef;
  $x;					# MBI bnorm is no-op, so dont call it
  } 
 
##############################################################################

sub as_hex
  {
  # return number as hexadecimal string (only for integers defined)
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $x->bstr() if $x->{sign} !~ /^[+-]$/;  # inf, nan etc
  return '0x0' if $x->is_zero();

  return $nan if $x->{_e}->{sign} ne '+';	# how to do 1e-1 in hex!?

  my $z = $x->{_m}->copy();
  if (!$x->{_e}->is_zero())		# > 0 
    {
    $z->blsft($x->{_e},10);
    }
  $z->{sign} = $x->{sign};
  $z->as_hex();
  }

sub as_bin
  {
  # return number as binary digit string (only for integers defined)
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return $x->bstr() if $x->{sign} !~ /^[+-]$/;  # inf, nan etc
  return '0b0' if $x->is_zero();

  return $nan if $x->{_e}->{sign} ne '+';	# how to do 1e-1 in hex!?

  my $z = $x->{_m}->copy();
  if (!$x->{_e}->is_zero())		# > 0 
    {
    $z->blsft($x->{_e},10);
    }
  $z->{sign} = $x->{sign};
  $z->as_bin();
  }

sub as_number
  {
  # return copy as a bigint representation of this BigFloat number
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  my $z = $x->{_m}->copy();
  if ($x->{_e}->{sign} eq '-')		# < 0
    {
    $x->{_e}->{sign} = '+';		# flip
    $z->brsft($x->{_e},10);
    $x->{_e}->{sign} = '-';		# flip back
    } 
  elsif (!$x->{_e}->is_zero())		# > 0 
    {
    $z->blsft($x->{_e},10);
    }
  $z->{sign} = $x->{sign};
  $z;
  }

sub length
  {
  my $x = shift;
  my $class = ref($x) || $x;
  $x = $class->new(shift) unless ref($x);

  return 1 if $x->{_m}->is_zero();
  my $len = $x->{_m}->length();
  $len += $x->{_e} if $x->{_e}->sign() eq '+';
  if (wantarray())
    {
    my $t = $MBI->bzero();
    $t = $x->{_e}->copy()->babs() if $x->{_e}->sign() eq '-';
    return ($len,$t);
    }
  $len;
  }

1;
__END__

=head1 NAME

Math::BigFloat - Arbitrary size floating point math package

=head1 SYNOPSIS

  use Math::BigFloat;

  # Number creation
  $x = Math::BigFloat->new($str);	# defaults to 0
  $nan  = Math::BigFloat->bnan();	# create a NotANumber
  $zero = Math::BigFloat->bzero();	# create a +0
  $inf = Math::BigFloat->binf();	# create a +inf
  $inf = Math::BigFloat->binf('-');	# create a -inf
  $one = Math::BigFloat->bone();	# create a +1
  $one = Math::BigFloat->bone('-');	# create a -1

  # Testing
  $x->is_zero();		# true if arg is +0
  $x->is_nan();			# true if arg is NaN
  $x->is_one();			# true if arg is +1
  $x->is_one('-');		# true if arg is -1
  $x->is_odd();			# true if odd, false for even
  $x->is_even();		# true if even, false for odd
  $x->is_positive();		# true if >= 0
  $x->is_negative();		# true if <  0
  $x->is_inf(sign);		# true if +inf, or -inf (default is '+')

  $x->bcmp($y);			# compare numbers (undef,<0,=0,>0)
  $x->bacmp($y);		# compare absolutely (undef,<0,=0,>0)
  $x->sign();			# return the sign, either +,- or NaN
  $x->digit($n);		# return the nth digit, counting from right
  $x->digit(-$n);		# return the nth digit, counting from left 

  # The following all modify their first argument. If you want to preserve
  # $x, use $z = $x->copy()->bXXX($y); See under L<CAVEATS> for why this is
  # neccessary when mixing $a = $b assigments with non-overloaded math.
 
  # set 
  $x->bzero();			# set $i to 0
  $x->bnan();			# set $i to NaN
  $x->bone();                   # set $x to +1
  $x->bone('-');                # set $x to -1
  $x->binf();                   # set $x to inf
  $x->binf('-');                # set $x to -inf

  $x->bneg();			# negation
  $x->babs();			# absolute value
  $x->bnorm();			# normalize (no-op)
  $x->bnot();			# two's complement (bit wise not)
  $x->binc();			# increment x by 1
  $x->bdec();			# decrement x by 1
  
  $x->badd($y);			# addition (add $y to $x)
  $x->bsub($y);			# subtraction (subtract $y from $x)
  $x->bmul($y);			# multiplication (multiply $x by $y)
  $x->bdiv($y);			# divide, set $x to quotient
				# return (quo,rem) or quo if scalar

  $x->bmod($y);			# modulus ($x % $y)
  $x->bpow($y);			# power of arguments ($x ** $y)
  $x->blsft($y);		# left shift
  $x->brsft($y);		# right shift 
				# return (quo,rem) or quo if scalar
  
  $x->blog();			# logarithm of $x to base e (Euler's number)
  $x->blog($base);		# logarithm of $x to base $base (f.i. 2)
  
  $x->band($y);			# bit-wise and
  $x->bior($y);			# bit-wise inclusive or
  $x->bxor($y);			# bit-wise exclusive or
  $x->bnot();			# bit-wise not (two's complement)
 
  $x->bsqrt();			# calculate square-root
  $x->broot($y);		# $y'th root of $x (e.g. $y == 3 => cubic root)
  $x->bfac();			# factorial of $x (1*2*3*4*..$x)
 
  $x->bround($N); 		# accuracy: preserve $N digits
  $x->bfround($N);		# precision: round to the $Nth digit

  $x->bfloor();			# return integer less or equal than $x
  $x->bceil();			# return integer greater or equal than $x

  # The following do not modify their arguments:

  bgcd(@values);		# greatest common divisor
  blcm(@values);		# lowest common multiplicator
  
  $x->bstr();			# return string
  $x->bsstr();			# return string in scientific notation
 
  $x->exponent();		# return exponent as BigInt
  $x->mantissa();		# return mantissa as BigInt
  $x->parts();			# return (mantissa,exponent) as BigInt

  $x->length();			# number of digits (w/o sign and '.')
  ($l,$f) = $x->length();	# number of digits, and length of fraction	

  $x->precision();		# return P of $x (or global, if P of $x undef)
  $x->precision($n);		# set P of $x to $n
  $x->accuracy();		# return A of $x (or global, if A of $x undef)
  $x->accuracy($n);		# set A $x to $n

  # these get/set the appropriate global value for all BigFloat objects
  Math::BigFloat->precision();	# Precision
  Math::BigFloat->accuracy();	# Accuracy
  Math::BigFloat->round_mode();	# rounding mode

=head1 DESCRIPTION

All operators (inlcuding basic math operations) are overloaded if you
declare your big floating point numbers as

  $i = new Math::BigFloat '12_3.456_789_123_456_789E-2';

Operations with overloaded operators preserve the arguments, which is
exactly what you expect.

=head2 Canonical notation

Input to these routines are either BigFloat objects, or strings of the
following four forms:

=over 2

=item *

C</^[+-]\d+$/>

=item *

C</^[+-]\d+\.\d*$/>

=item *

C</^[+-]\d+E[+-]?\d+$/>

=item *

C</^[+-]\d*\.\d+E[+-]?\d+$/>

=back

all with optional leading and trailing zeros and/or spaces. Additonally,
numbers are allowed to have an underscore between any two digits.

Empty strings as well as other illegal numbers results in 'NaN'.

bnorm() on a BigFloat object is now effectively a no-op, since the numbers 
are always stored in normalized form. On a string, it creates a BigFloat 
object.

=head2 Output

Output values are BigFloat objects (normalized), except for bstr() and bsstr().

The string output will always have leading and trailing zeros stripped and drop
a plus sign. C<bstr()> will give you always the form with a decimal point,
while C<bsstr()> (s for scientific) gives you the scientific notation.

	Input			bstr()		bsstr()
	'-0'			'0'		'0E1'
   	'  -123 123 123'	'-123123123'	'-123123123E0'
	'00.0123'		'0.0123'	'123E-4'
	'123.45E-2'		'1.2345'	'12345E-4'
	'10E+3'			'10000'		'1E4'

Some routines (C<is_odd()>, C<is_even()>, C<is_zero()>, C<is_one()>,
C<is_nan()>) return true or false, while others (C<bcmp()>, C<bacmp()>)
return either undef, <0, 0 or >0 and are suited for sort.

Actual math is done by using the class defined with C<with => Class;> (which
defaults to BigInts) to represent the mantissa and exponent.

The sign C</^[+-]$/> is stored separately. The string 'NaN' is used to 
represent the result when input arguments are not numbers, as well as 
the result of dividing by zero.

=head2 C<mantissa()>, C<exponent()> and C<parts()>

C<mantissa()> and C<exponent()> return the said parts of the BigFloat 
as BigInts such that:

	$m = $x->mantissa();
	$e = $x->exponent();
	$y = $m * ( 10 ** $e );
	print "ok\n" if $x == $y;

C<< ($m,$e) = $x->parts(); >> is just a shortcut giving you both of them.

A zero is represented and returned as C<0E1>, B<not> C<0E0> (after Knuth).

Currently the mantissa is reduced as much as possible, favouring higher
exponents over lower ones (e.g. returning 1e7 instead of 10e6 or 10000000e0).
This might change in the future, so do not depend on it.

=head2 Accuracy vs. Precision

See also: L<Rounding|Rounding>.

Math::BigFloat supports both precision and accuracy. For a full documentation,
examples and tips on these topics please see the large section in
L<Math::BigInt>.

Since things like sqrt(2) or 1/3 must presented with a limited precision lest
a operation consumes all resources, each operation produces no more than
the requested number of digits.

Please refer to BigInt's documentation for the precedence rules of which
accuracy/precision setting will be used.

If there is no gloabl precision set, B<and> the operation inquestion was not
called with a requested precision or accuracy, B<and> the input $x has no
accuracy or precision set, then a fallback parameter will be used. For
historical reasons, it is called C<div_scale> and can be accessed via:

	$d = Math::BigFloat->div_scale();		# query
	Math::BigFloat->div_scale($n);			# set to $n digits

The default value is 40 digits.

In case the result of one operation has more precision than specified,
it is rounded. The rounding mode taken is either the default mode, or the one
supplied to the operation after the I<scale>:

	$x = Math::BigFloat->new(2);
	Math::BigFloat->precision(5);		# 5 digits max
	$y = $x->copy()->bdiv(3);		# will give 0.66666
	$y = $x->copy()->bdiv(3,6);		# will give 0.666666
	$y = $x->copy()->bdiv(3,6,'odd');	# will give 0.666667
	Math::BigFloat->round_mode('zero');
	$y = $x->copy()->bdiv(3,6);		# will give 0.666666

=head2 Rounding

=over 2

=item ffround ( +$scale )

Rounds to the $scale'th place left from the '.', counting from the dot.
The first digit is numbered 1. 

=item ffround ( -$scale )

Rounds to the $scale'th place right from the '.', counting from the dot.

=item ffround ( 0 )

Rounds to an integer.

=item fround  ( +$scale )

Preserves accuracy to $scale digits from the left (aka significant digits)
and pads the rest with zeros. If the number is between 1 and -1, the
significant digits count from the first non-zero after the '.'

=item fround  ( -$scale ) and fround ( 0 )

These are effectively no-ops.

=back

All rounding functions take as a second parameter a rounding mode from one of
the following: 'even', 'odd', '+inf', '-inf', 'zero' or 'trunc'.

The default rounding mode is 'even'. By using
C<< Math::BigFloat->round_mode($round_mode); >> you can get and set the default
mode for subsequent rounding. The usage of C<$Math::BigFloat::$round_mode> is
no longer supported.
The second parameter to the round functions then overrides the default
temporarily. 

The C<as_number()> function returns a BigInt from a Math::BigFloat. It uses
'trunc' as rounding mode to make it equivalent to:

	$x = 2.5;
	$y = int($x) + 2;

You can override this by passing the desired rounding mode as parameter to
C<as_number()>:

	$x = Math::BigFloat->new(2.5);
	$y = $x->as_number('odd');	# $y = 3

=head1 EXAMPLES
 
  # not ready yet

=head1 Autocreating constants

After C<use Math::BigFloat ':constant'> all the floating point constants
in the given scope are converted to C<Math::BigFloat>. This conversion
happens at compile time.

In particular

  perl -MMath::BigFloat=:constant -e 'print 2E-100,"\n"'

prints the value of C<2E-100>. Note that without conversion of 
constants the expression 2E-100 will be calculated as normal floating point 
number.

Please note that ':constant' does not affect integer constants, nor binary 
nor hexadecimal constants. Use L<bignum> or L<Math::BigInt> to get this to
work.

=head2 Math library

Math with the numbers is done (by default) by a module called
Math::BigInt::Calc. This is equivalent to saying:

	use Math::BigFloat lib => 'Calc';

You can change this by using:

	use Math::BigFloat lib => 'BitVect';

The following would first try to find Math::BigInt::Foo, then
Math::BigInt::Bar, and when this also fails, revert to Math::BigInt::Calc:

	use Math::BigFloat lib => 'Foo,Math::BigInt::Bar';

Calc.pm uses as internal format an array of elements of some decimal base
(usually 1e7, but this might be differen for some systems) with the least
significant digit first, while BitVect.pm uses a bit vector of base 2, most
significant bit first. Other modules might use even different means of
representing the numbers. See the respective module documentation for further
details.

Please note that Math::BigFloat does B<not> use the denoted library itself,
but it merely passes the lib argument to Math::BigInt. So, instead of the need
to do:

	use Math::BigInt lib => 'GMP';
	use Math::BigFloat;

you can roll it all into one line:

	use Math::BigFloat lib => 'GMP';

It is also possible to just require Math::BigFloat:

	require Math::BigFloat;

This will load the neccessary things (like BigInt) when they are needed, and
automatically.

Use the lib, Luke! And see L<Using Math::BigInt::Lite> for more details than
you ever wanted to know about loading a different library.

=head2 Using Math::BigInt::Lite

It is possible to use L<Math::BigInt::Lite> with Math::BigFloat:

        # 1
        use Math::BigFloat with => 'Math::BigInt::Lite';

There is no need to "use Math::BigInt" or "use Math::BigInt::Lite", but you
can combine these if you want. For instance, you may want to use
Math::BigInt objects in your main script, too.

        # 2
        use Math::BigInt;
        use Math::BigFloat with => 'Math::BigInt::Lite';

Of course, you can combine this with the C<lib> parameter.

        # 3
        use Math::BigFloat with => 'Math::BigInt::Lite', lib => 'GMP,Pari';

There is no need for a "use Math::BigInt;" statement, even if you want to
use Math::BigInt's, since Math::BigFloat will needs Math::BigInt and thus
always loads it. But if you add it, add it B<before>:

        # 4
        use Math::BigInt;
        use Math::BigFloat with => 'Math::BigInt::Lite', lib => 'GMP,Pari';

Notice that the module with the last C<lib> will "win" and thus
it's lib will be used if the lib is available:

        # 5
        use Math::BigInt lib => 'Bar,Baz';
        use Math::BigFloat with => 'Math::BigInt::Lite', lib => 'Foo';

That would try to load Foo, Bar, Baz and Calc (in that order). Or in other
words, Math::BigFloat will try to retain previously loaded libs when you
don't specify it onem but if you specify one, it will try to load them.

Actually, the lib loading order would be "Bar,Baz,Calc", and then
"Foo,Bar,Baz,Calc", but independend of which lib exists, the result is the
same as trying the latter load alone, except for the fact that one of Bar or
Baz might be loaded needlessly in an intermidiate step (and thus hang around
and waste memory). If neither Bar nor Baz exist (or don't work/compile), they
will still be tried to be loaded, but this is not as time/memory consuming as
actually loading one of them. Still, this type of usage is not recommended due
to these issues.

The old way (loading the lib only in BigInt) still works though:

        # 6
        use Math::BigInt lib => 'Bar,Baz';
        use Math::BigFloat;

You can even load Math::BigInt afterwards:

        # 7
        use Math::BigFloat;
        use Math::BigInt lib => 'Bar,Baz';

But this has the same problems like #5, it will first load Calc
(Math::BigFloat needs Math::BigInt and thus loads it) and then later Bar or
Baz, depending on which of them works and is usable/loadable. Since this
loads Calc unnecc., it is not recommended.

Since it also possible to just require Math::BigFloat, this poses the question
about what libary this will use:

	require Math::BigFloat;
	my $x = Math::BigFloat->new(123); $x += 123;

It will use Calc. Please note that the call to import() is still done, but
only when you use for the first time some Math::BigFloat math (it is triggered
via any constructor, so the first time you create a Math::BigFloat, the load
will happen in the background). This means:

	require Math::BigFloat;
	Math::BigFloat->import ( lib => 'Foo,Bar' );

would be the same as:

	use Math::BigFloat lib => 'Foo, Bar';

But don't try to be clever to insert some operations in between:

	require Math::BigFloat;
	my $x = Math::BigFloat->bone() + 4;		# load BigInt and Calc
	Math::BigFloat->import( lib => 'Pari' );	# load Pari, too
	$x = Math::BigFloat->bone()+4;			# now use Pari

While this works, it loads Calc needlessly. But maybe you just wanted that?

B<Examples #3 is highly recommended> for daily usage.

=head1 BUGS

Please see the file BUGS in the CPAN distribution Math::BigInt for known bugs.

=head1 CAVEATS

=over 1

=item stringify, bstr()

Both stringify and bstr() now drop the leading '+'. The old code would return
'+1.23', the new returns '1.23'. See the documentation in L<Math::BigInt> for
reasoning and details.

=item bdiv

The following will probably not do what you expect:

	print $c->bdiv(123.456),"\n";

It prints both quotient and reminder since print works in list context. Also,
bdiv() will modify $c, so be carefull. You probably want to use
	
	print $c / 123.456,"\n";
	print scalar $c->bdiv(123.456),"\n";  # or if you want to modify $c

instead.

=item Modifying and =

Beware of:

	$x = Math::BigFloat->new(5);
	$y = $x;

It will not do what you think, e.g. making a copy of $x. Instead it just makes
a second reference to the B<same> object and stores it in $y. Thus anything
that modifies $x will modify $y (except overloaded math operators), and vice
versa. See L<Math::BigInt> for details and how to avoid that.

=item bpow

C<bpow()> now modifies the first argument, unlike the old code which left
it alone and only returned the result. This is to be consistent with
C<badd()> etc. The first will modify $x, the second one won't:

	print bpow($x,$i),"\n"; 	# modify $x
	print $x->bpow($i),"\n"; 	# ditto
	print $x ** $i,"\n";		# leave $x alone 

=back

=head1 SEE ALSO

L<Math::BigInt>, L<Math::BigRat> and L<Math::Big> as well as
L<Math::BigInt::BitVect>, L<Math::BigInt::Pari> and  L<Math::BigInt::GMP>.

The pragmas L<bignum>, L<bigint> and L<bigrat> might also be of interest
because they solve the autoupgrading/downgrading issue, at least partly.

The package at
L<http://search.cpan.org/search?mode=module&query=Math%3A%3ABigInt> contains
more documentation including a full version history, testcases, empty
subclass files and benchmarks.

=head1 LICENSE

This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=head1 AUTHORS

Mark Biggar, overloaded interface by Ilya Zakharevich.
Completely rewritten by Tels http://bloodgate.com in 2001, 2002, and still
at it in 2003.

=cut
