package Math::BigFloat;

# 
# Mike grinned. 'Two down, infinity to go' - Mike Nostrus in 'Before and After'
#

# The following hash values are internally used:
#   _e: exponent (BigInt)
#   _m: mantissa (absolute BigInt)
# sign: +,-,"NaN" if not a number
#   _a: accuracy
#   _p: precision
#   _f: flags, used to signal MBI not to touch our private parts

$VERSION = '1.35';
require 5.005;
use Exporter;
use File::Spec;
# use Math::BigInt;
@ISA =       qw( Exporter Math::BigInt);

use strict;
use vars qw/$AUTOLOAD $accuracy $precision $div_scale $round_mode $rnd_mode/;
use vars qw/$upgrade $downgrade/;
my $class = "Math::BigFloat";

use overload
'<=>'	=>	sub { $_[2] ?
                      ref($_[0])->bcmp($_[1],$_[0]) : 
                      ref($_[0])->bcmp($_[0],$_[1])},
'int'	=>	sub { $_[0]->as_number() },		# 'trunc' to bigint
;

##############################################################################
# global constants, flags and accessory

use constant MB_NEVER_ROUND => 0x0001;

# are NaNs ok?
my $NaNOK=1;
# constant for easier life
my $nan = 'NaN'; 

# class constants, use Class->constant_name() to access
$round_mode = 'even'; # one of 'even', 'odd', '+inf', '-inf', 'zero' or 'trunc'
$accuracy   = undef;
$precision  = undef;
$div_scale  = 40;

$upgrade = undef;
$downgrade = undef;
my $MBI = 'Math::BigInt'; # the package we are using for our private parts
			  # changable by use Math::BigFloat with => 'package'

##############################################################################
# the old code had $rnd_mode, so we need to support it, too

sub TIESCALAR   { my ($class) = @_; bless \$round_mode, $class; }
sub FETCH       { return $round_mode; }
sub STORE       { $rnd_mode = $_[0]->round_mode($_[1]); }

BEGIN
  { 
  $rnd_mode   = 'even';
  tie $rnd_mode, 'Math::BigFloat'; 
  }
 
##############################################################################

# in case we call SUPER::->foo() and this wants to call modify()
# sub modify () { 0; }

{
  # valid method aliases for AUTOLOAD
  my %methods = map { $_ => 1 }  
   qw / fadd fsub fmul fdiv fround ffround fsqrt fmod fstr fsstr fpow fnorm
        fint facmp fcmp fzero fnan finf finc fdec flog ffac
	fceil ffloor frsft flsft fone flog
      /;
  # valid method's that can be hand-ed up (for AUTOLOAD)
  my %hand_ups = map { $_ => 1 }  
   qw / is_nan is_inf is_negative is_positive
        accuracy precision div_scale round_mode fneg fabs babs fnot
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
    die "$wanted is not a number initialized to $class" if !$NaNOK;
    
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
    # 3.123E0 = 3123E-3, and 3.123E-2 => 3123E-5
    $self->{_e} -= CORE::length($$mfv) if CORE::length($$mfv) != 0; 		
    $self->{sign} = $$mis;
    }
  # if downgrade, inf, NaN or integers go down

  if ($downgrade && $self->{_e}->{sign} eq '+')
    {
#   print "downgrading $$miv$$mfv"."E$$es$$ev";
    if ($self->{_e}->is_zero())
      {
      $self->{_m}->{sign} = $$mis;		# negative if wanted
      return $downgrade->new($self->{_m});
      }
    return $downgrade->new("$$mis$$miv$$mfv"."E$$es$$ev");
    }
  # print "mbf new $self->{sign} $self->{_m} e $self->{_e} ",ref($self),"\n";
  $self->bnorm()->round(@r);		# first normalize, then round
  }

sub _bnan
  {
  # used by parent class bone() to initialize number to 1
  my $self = shift;
  $self->{_m} = $MBI->bzero();
  $self->{_e} = $MBI->bzero();
  }

sub _binf
  {
  # used by parent class bone() to initialize number to 1
  my $self = shift;
  $self->{_m} = $MBI->bzero();
  $self->{_e} = $MBI->bzero();
  }

sub _bone
  {
  # used by parent class bone() to initialize number to 1
  my $self = shift;
  $self->{_m} = $MBI->bone();
  $self->{_e} = $MBI->bzero();
  }

sub _bzero
  {
  # used by parent class bone() to initialize number to 1
  my $self = shift;
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

  my $cfg = $MBI->config();

  no strict 'refs';
  $cfg->{class} = $class;
  $cfg->{with} = $MBI;
  foreach (
   qw/upgrade downgrade precision accuracy round_mode VERSION div_scale/)
    {
    $cfg->{lc($_)} = ${"${class}::$_"};
    };
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
  #my $x = shift; my $class = ref($x) || $x;
  #$x = $class->new(shift) unless ref($x);

  #die "Oups! e was $nan" if $x->{_e}->{sign} eq $nan;
  #die "Oups! m was $nan" if $x->{_m}->{sign} eq $nan;
  if ($x->{sign} !~ /^[+-]$/)
    {
    return $x->{sign} unless $x->{sign} eq '+inf';      # -inf, NaN
    return 'inf';                                       # +inf
    }
 
  my $es = '0'; my $len = 1; my $cad = 0; my $dot = '.';

  my $not_zero = ! $x->is_zero();
  if ($not_zero)
    {
    $es = $x->{_m}->bstr();
    $len = CORE::length($es);
    if (!$x->{_e}->is_zero())
      {
      if ($x->{_e}->sign() eq '-')
        {
        $dot = '';
        if ($x->{_e} <= -$len)
          {
          # print "style: 0.xxxx\n";
          my $r = $x->{_e}->copy(); $r->babs()->bsub( CORE::length($es) );
          $es = '0.'. ('0' x $r) . $es; $cad = -($len+$r);
          }
        else
          {
          # print "insert '.' at $x->{_e} in '$es'\n";
          substr($es,$x->{_e},0) = '.'; $cad = $x->{_e};
          }
        }
      else
        {
        # expand with zeros
        $es .= '0' x $x->{_e}; $len += $x->{_e}; $cad = 0;
        }
      }
    } # if not zero
  $es = $x->{sign}.$es if $x->{sign} eq '-';
  # if set accuracy or precision, pad with zeros
  if ((defined $x->{_a}) && ($not_zero))
    {
    # 123400 => 6, 0.1234 => 4, 0.001234 => 4
    my $zeros = $x->{_a} - $cad;		# cad == 0 => 12340
    $zeros = $x->{_a} - $len if $cad != $len;
    $es .= $dot.'0' x $zeros if $zeros > 0;
    }
  elsif ($x->{_p} || 0 < 0)
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
  #my $x = shift; my $class = ref($x) || $x;
  #$x = $class->new(shift) unless ref($x);

  #die "Oups! e was $nan" if $x->{_e}->{sign} eq $nan;
  #die "Oups! m was $nan" if $x->{_m}->{sign} eq $nan;
  if ($x->{sign} !~ /^[+-]$/)
    {
    return $x->{sign} unless $x->{sign} eq '+inf';      # -inf, NaN
    return 'inf';                                       # +inf
    }
  my $sign = $x->{_e}->{sign}; $sign = '' if $sign eq '-';
  my $sep = 'e'.$sign;
  $x->{_m}->bstr().$sep.$x->{_e}->bstr();
  }
    
sub numify 
  {
  # Make a number from a BigFloat object
  # simple return string and let Perl's atoi()/atof() handle the rest
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);
  $x->bsstr(); 
  }

##############################################################################
# public stuff (usually prefixed with "b")

# tels 2001-08-04 
# todo: this must be overwritten and return NaN for non-integer values
# band(), bior(), bxor(), too
#sub bnot
#  {
#  $class->SUPER::bnot($class,@_);
#  }

sub bcmp 
  {
  # Compares 2 values.  Returns one of undef, <0, =0, >0. (suitable for sort)
  # (BFLOAT or num_str, BFLOAT or num_str) return cond_code

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
  # (BFLOAT or num_str, BFLOAT or num_str) return cond_code
  
  # set up parameters
  my ($self,$x,$y) = (ref($_[0]),@_);
  # objectify is costly, so avoid it
  if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1])))
    {
    ($self,$x,$y) = objectify(2,@_);
    }

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
  $e = $MBI->bzero() if !defined $e;	# if no BFLOAT ?
  $e = $e->copy();			# make copy (didn't do it yet)
  $e->bsub($x->{_e});
  my $add = $y->{_m}->copy();
  if ($e->{sign} eq '-')		# < 0
    {
    my $e1 = $e->copy()->babs();
    #$x->{_m} *= (10 ** $e1);
    $x->{_m}->blsft($e1,10);
    $x->{_e} += $e;			# need the sign of e
    }
  elsif (!$e->is_zero())		# > 0
    {
    #$add *= (10 ** $e);
    $add->blsft($e,10);
    }
  # else: both e are the same, so just leave them
  $x->{_m}->{sign} = $x->{sign}; 		# fiddle with signs
  $add->{sign} = $y->{sign};
  $x->{_m} += $add; 				# finally do add/sub
  $x->{sign} = $x->{_m}->{sign}; 		# re-adjust signs
  $x->{_m}->{sign} = '+';			# mantissa always positiv
  # delete trailing zeros, then round
  return $x->bnorm()->round($a,$p,$r,$y);
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
  
  $y->{sign} =~ tr/+\-/-+/;	# does nothing for NaN
  $x->badd($y,$a,$p,$r);	# badd does not leave internal zeros
  $y->{sign} =~ tr/+\-/-+/;	# refix $y (does nothing for NaN)
  $x;				# already rounded by badd()
  }

sub binc
  {
  # increment arg by one
  my ($self,$x,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  if ($x->{_e}->sign() eq '-')
    {
    return $x->badd($self->bone(),$a,$p,$r);	#  digits after dot
    }

  if (!$x->{_e}->is_zero())
    {
    $x->{_m}->blsft($x->{_e},10);		# 1e2 => 100
    $x->{_e}->bzero();
    }
  # now $x->{_e} == 0
  if ($x->{sign} eq '+')
    {
    $x->{_m}->binc();
    return $x->bnorm()->bround($a,$p,$r);
    }
  elsif ($x->{sign} eq '-')
    {
    $x->{_m}->bdec();
    $x->{sign} = '+' if $x->{_m}->is_zero(); # -1 +1 => -0 => +0
    return $x->bnorm()->bround($a,$p,$r);
    }
  # inf, nan handling etc
  $x->badd($self->__one(),$a,$p,$r);		# does round 
  }

sub bdec
  {
  # decrement arg by one
  my ($self,$x,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  if ($x->{_e}->sign() eq '-')
    {
    return $x->badd($self->bone('-'),$a,$p,$r);	#  digits after dot
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
    return $x->bnorm()->round($a,$p,$r);
    }
  # > 0
  elsif ($x->{sign} eq '+')
    {
    $x->{_m}->bdec();
    return $x->bnorm()->round($a,$p,$r);
    }
  # inf, nan handling etc
  $x->badd($self->bone('-'),$a,$p,$r);		# does round 
  } 

sub blog
  {
  my ($self,$x,$base,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(2,@_);

  # http://www.efunda.com/math/taylor_series/logarithmic.cfm?search_string=log

  # u = x-1, v = x+1
  #              _                               _
  # Taylor:     |    u    1   u^3   1   u^5       |
  # ln (x)  = 2 |   --- + - * --- + - * --- + ... |  x > 0
  #             |_   v    3   v^3   5   v^5      _|

  # This takes much more steps to calculate the result: 
  # u = x-1
  #              _                               _
  # Taylor:     |    u    1   u^2   1   u^3       |
  # ln (x)  = 2 |   --- + - * --- + - * --- + ... |  x > 1/2
  #             |_   x    2   x^2   3   x^3      _|

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my $scale = 0;
  my @params = $x->_find_round_parameters($a,$p,$r);

  # no rounding at all, so must use fallback
  if (scalar @params == 1)
    {
    # simulate old behaviour
    $params[1] = $self->div_scale();	# and round to it as accuracy
    $params[0] = undef;
    $scale = $params[1]+4; 		# at least four more for proper round
    $params[3] = $r;			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[1] || $params[2]) + 4;	# take whatever is defined
    }

  return $x->bzero(@params) if $x->is_one();
  return $x->bnan() if $x->{sign} ne '+' || $x->is_zero();
  return $x->bone('+',@params) if $x->bcmp($base) == 0;

  # when user set globals, they would interfere with our calculation, so
  # disable then and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;
 
  my ($case,$limit,$v,$u,$below,$factor,$two,$next,$over,$f);

  if (3 < 5)
  #if ($x <= Math::BigFloat->new("0.5"))
    {
    $case = 0;
  #  print "case $case $x < 0.5\n";
    $v = $x->copy(); $v->binc();		# v = x+1
    $x->bdec(); $u = $x->copy();		# u = x-1; x = x-1
    $x->bdiv($v,$scale);			# first term: u/v
    $below = $v->copy();
    $over = $u->copy();
    $u *= $u; $v *= $v;				# u^2, v^2
    $below->bmul($v);				# u^3, v^3
    $over->bmul($u);
    $factor = $self->new(3); $f = $self->new(2);
    }
  #else
  #  {
  #  $case = 1;
  #  print "case 1 $x > 0.5\n";
  #  $v = $x->copy();				# v = x
  #  $u = $x->copy(); $u->bdec();		# u = x-1;
  #  $x->bdec(); $x->bdiv($v,$scale);		# first term: x-1/x
  #  $below = $v->copy();
  #  $over = $u->copy();
  #  $below->bmul($v);				# u^2, v^2
  #  $over->bmul($u);
  #  $factor = $self->new(2); $f = $self->bone();
  #  }
  $limit = $self->new("1E-". ($scale-1));
  #my $steps = 0;
  while (3 < 5)
    {
    # we calculate the next term, and add it to the last
    # when the next term is below our limit, it won't affect the outcome
    # anymore, so we stop
    $next = $over->copy()->bdiv($below->copy()->bmul($factor),$scale);
    last if $next->bcmp($limit) <= 0;
    $x->badd($next);
    # print "step  $x\n";
    # calculate things for the next term
    $over *= $u; $below *= $v; $factor->badd($f);
    #$steps++;
    }
  $x->bmul(2) if $case == 0;
  #print "took $steps steps\n";
  
  # shortcut to not run trough _find_round_parameters again
  if (defined $params[1])
    {
    $x->bround($params[1],$params[3]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[2],$params[3]);		# then round accordingly
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

sub is_int
  {
  # return true if arg (BFLOAT or num_str) is an integer
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return 1 if ($x->{sign} =~ /^[+-]$/) &&	# NaN and +-inf aren't
    $x->{_e}->{sign} eq '+';			# 1e-1 => no integer
  0;
  }

sub is_zero
  {
  # return true if arg (BFLOAT or num_str) is zero
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  return 1 if $x->{sign} eq '+' && $x->{_m}->is_zero();
  0;
  }

sub is_one
  {
  # return true if arg (BFLOAT or num_str) is +1 or -1 if signis given
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

  my $sign = shift || ''; $sign = '+' if $sign ne '-';
  return 1
   if ($x->{sign} eq $sign && $x->{_e}->is_zero() && $x->{_m}->is_one()); 
  0;
  }

sub is_odd
  {
  # return true if arg (BFLOAT or num_str) is odd or false if even
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);
  
  return 1 if ($x->{sign} =~ /^[+-]$/) &&		# NaN & +-inf aren't
    ($x->{_e}->is_zero() && $x->{_m}->is_odd()); 
  0;
  }

sub is_even
  {
  # return true if arg (BINT or num_str) is even or false if odd
  my ($self,$x) = ref($_[0]) ? (ref($_[0]),$_[0]) : objectify(1,@_);

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
  my $scale = 0;
  my @params = $x->_find_round_parameters($a,$p,$r,$y);

  # no rounding at all, so must use fallback
  if (scalar @params == 1)
    {
    # simulate old behaviour
    $params[1] = $self->div_scale();	# and round to it as accuracy
    $scale = $params[1]+4; 		# at least four more for proper round
    $params[3] = $r;			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[1] || $params[2]) + 4;	# take whatever is defined
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

    #print "bdiv $y ",ref($y),"\n";
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

  # shortcut to not run trough _find_round_parameters again
  if (defined $params[1])
    {
    $x->bround($params[1],$params[3]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[2],$params[3]);		# then round accordingly
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
      $rem->bmod($y,$params[1],$params[2],$params[3]);	# copy already done
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

sub bsqrt
  { 
  # calculate square root; this should probably
  # use a different test to see whether the accuracy we want is...
  my ($self,$x,$a,$p,$r) = ref($_[0]) ? (ref($_[0]),@_) : objectify(1,@_);

  return $x->bnan() if $x->{sign} eq 'NaN' || $x->{sign} =~ /^-/; # <0, NaN
  return $x if $x->{sign} eq '+inf';				  # +inf
  return $x if $x->is_zero() || $x->is_one();

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my $scale = 0;
  my @params = $x->_find_round_parameters($a,$p,$r);

  # no rounding at all, so must use fallback
  if (scalar @params == 1)
    {
    # simulate old behaviour
    $params[1] = $self->div_scale();	# and round to it as accuracy
    $scale = $params[1]+4; 		# at least four more for proper round
    $params[3] = $r;			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[1] || $params[2]) + 4;	# take whatever is defined
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

#  print "guess $gs\n";
  if (($x->{_e}->{sign} ne '-')		# guess can't be accurate if there are
					# digits after the dot
   && ($xas->bacmp($gs * $gs) == 0))	# guess hit the nail on the head?
    {
    # exact result
    $x->{_m} = $gs; $x->{_e} = $MBI->bzero(); $x->bnorm();
    # shortcut to not run trough _find_round_parameters again
    if (defined $params[1])
      {
      $x->bround($params[1],$params[3]);	# then round accordingly
      }
    else
      {
      $x->bfround($params[2],$params[3]);	# then round accordingly
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
  $gs = $self->new( $gs );		# BigInt to BigFloat

  my $lx = $x->{_m}->length();
  $scale = $lx if $scale < $lx;
  my $e = $self->new("1E-$scale");	# make test variable

  my $y = $x->copy();
  my $two = $self->new(2);
  my $diff = $e;
  # promote BigInts and it's subclasses (except when already a BigFloat)
  $y = $self->new($y) unless $y->isa('Math::BigFloat'); 

  my $rem;
  while ($diff->bacmp($e) >= 0)
    {
    $rem = $y->copy()->bdiv($gs,$scale);
    $rem = $y->copy()->bdiv($gs,$scale)->badd($gs)->bdiv($two,$scale);
    $diff = $rem->copy()->bsub($gs);
    $gs = $rem->copy();
    }
  # copy over to modify $x
  $x->{_m} = $rem->{_m}; $x->{_e} = $rem->{_e};
  
  # shortcut to not run trough _find_round_parameters again
  if (defined $params[1])
    {
    $x->bround($params[1],$params[3]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[2],$params[3]);		# then round accordingly
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
  # compute factorial numbers
  # modifies first argument
  my ($self,$x,@r) = objectify(1,@_);

  return $x->bnan() 
    if (($x->{sign} ne '+') ||		# inf, NaN, <0 etc => NaN
     ($x->{_e}->{sign} ne '+'));	# digits after dot?

  return $x->bone('+',@r) if $x->is_zero() || $x->is_one();	# 0 or 1 => 1
  
  # use BigInt's bfac() for faster calc
  $x->{_m}->blsft($x->{_e},10);		# un-norm m
  $x->{_e}->bzero();			# norm $x again
  $x->{_m}->bfac();			# factorial
  $x->bnorm()->round(@r);
  }

sub _pow2
  {
  # Calculate a power where $y is a non-integer, like 2 ** 0.5
  my ($x,$y,$a,$p,$r) = @_;
  my $self = ref($x);
  
  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my $scale = 0;
  my @params = $x->_find_round_parameters($a,$p,$r);

  # no rounding at all, so must use fallback
  if (scalar @params == 1)
    {
    # simulate old behaviour
    $params[1] = $self->div_scale();	# and round to it as accuracy
    $scale = $params[1]+4; 		# at least four more for proper round
    $params[3] = $r;			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[1] || $params[2]) + 4;	# take whatever is defined
    }

  # when user set globals, they would interfere with our calculation, so
  # disable then and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;
 
  # split the second argument into its integer and fraction part
  # we calculate the result then from these two parts, like in
  # 2 ** 2.4 == (2 ** 2) * (2 ** 0.4)
  my $c = $self->new($y->as_number());	# integer part
  my $d = $y-$c;			# fractional part
  my $xc = $x->copy();			# a temp. copy
  
  # now calculate binary fraction from the decimal fraction on the fly
  # f.i. 0.654:
  # 0.654 * 2 = 1.308 > 1 => 0.1	( 1.308 - 1 = 0.308)
  # 0.308 * 2 = 0.616 < 1 => 0.10
  # 0.616 * 2 = 1.232 > 1 => 0.101	( 1.232 - 1 = 0.232)
  # and so on...
  # The process stops when the result is exactly one, or when we have
  # enough accuracy

  # From the binary fraction we calculate the result as follows:
  # we assume the fraction ends in 1, and we remove this one first.
  # For each digit after the dot, assume 1 eq R and 0 eq XR, where R means
  # take square root and X multiply with the original X. 
  
  my $i = 0;
  while ($i++ < 50)
    {
    $d->badd($d);						# * 2
    last if $d->is_one();					# == 1
    $x->bsqrt();						# 0
    if ($d > 1)
      {
      $x->bsqrt(); $x->bmul($xc); $d->bdec();			# 1
      }
    }
  # assume fraction ends in 1
  $x->bsqrt();							# 1
  if (!$c->is_one())
    {
    $x->bmul( $xc->bpow($c) );
    }
  elsif (!$c->is_zero())
    {
    $x->bmul( $xc );
    }
  # done

  # shortcut to not run trough _find_round_parameters again
  if (defined $params[1])
    {
    $x->bround($params[1],$params[3]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[2],$params[3]);		# then round accordingly
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

sub _pow
  {
  # Calculate a power where $y is a non-integer, like 2 ** 0.5
  my ($x,$y,$a,$p,$r) = @_;
  my $self = ref($x);

  # if $y == 0.5, it is sqrt($x)
  return $x->bsqrt($a,$p,$r,$y) if $y->bcmp('0.5') == 0;

  # u = y * ln x
  #                _                             _
  # Taylor:       |    u     u^2      u^3         |
  # x ** y  = 1 + |   --- +  --- + * ----- + ...  |
  #               |_   1     1*2     1*2*3       _|

  # we need to limit the accuracy to protect against overflow
  my $fallback = 0;
  my $scale = 0;
  my @params = $x->_find_round_parameters($a,$p,$r);

  # no rounding at all, so must use fallback
  if (scalar @params == 1)
    {
    # simulate old behaviour
    $params[1] = $self->div_scale();	# and round to it as accuracy
    $scale = $params[1]+4; 		# at least four more for proper round
    $params[3] = $r;			# round mode by caller or undef
    $fallback = 1;			# to clear a/p afterwards
    }
  else
    {
    # the 4 below is empirical, and there might be cases where it is not
    # enough...
    $scale = abs($params[1] || $params[2]) + 4;	# take whatever is defined
    }

  # when user set globals, they would interfere with our calculation, so
  # disable then and later re-enable them
  no strict 'refs';
  my $abr = "$self\::accuracy"; my $ab = $$abr; $$abr = undef;
  my $pbr = "$self\::precision"; my $pb = $$pbr; $$pbr = undef;
  # we also need to disable any set A or P on $x (_find_round_parameters took
  # them already into account), since these would interfere, too
  delete $x->{_a}; delete $x->{_p};
  # need to disable $upgrade in BigInt, to avoid deep recursion
  local $Math::BigInt::upgrade = undef;
 
  my ($limit,$v,$u,$below,$factor,$next,$over);

  $u = $x->copy()->blog($scale)->bmul($y);
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
    last if $next->bcmp($limit) <= 0;
    $x->badd($next);
#    print "at $x\n";
    # calculate things for the next term
    $over *= $u; $below *= $factor; $factor->binc();
    #$steps++;
    }
  
  # shortcut to not run trough _find_round_parameters again
  if (defined $params[1])
    {
    $x->bround($params[1],$params[3]);		# then round accordingly
    }
  else
    {
    $x->bfround($params[2],$params[3]);		# then round accordingly
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
  
  die ('bround() needs positive accuracy') if ($_[0] || 0) < 0;

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
  # going through AUTOLOAD for every DESTROY is costly, so avoid it by empty sub
  }

sub AUTOLOAD
  {
  # make fxxx and bxxx both work by selectively mapping fxxx() to MBF::bxxx()
  # or falling back to MBI::bxxx()
  my $name = $AUTOLOAD;

  $name =~ s/.*:://;	# split package
  no strict 'refs';
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
  for ( my $i = 0; $i < $l ; $i++)
    {
#    print "at $_[$i] (",$_[$i+1]||'undef',")\n";
    if ( $_[$i] eq ':constant' )
      {
      # this rest causes overlord er load to step in
      # print "overload @_\n";
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
      $lib = $_[$i+1] || '';		# default Calc
      $i++;
      }
    elsif ($_[$i] eq 'with')
      {
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
  die ("Couldn't load $MBI: $! $@") if $@;

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

#  if (!$x->{_m}->is_odd())
#    {
    my $zeros = $x->{_m}->_trailing_zeros();	# correct for trailing zeros 
    if ($zeros != 0)
      {
      $x->{_m}->brsft($zeros,10); $x->{_e}->badd($zeros);
      }
    # for something like 0Ey, set y to 1, and -0 => +0
    $x->{sign} = '+', $x->{_e}->bone() if $x->{_m}->is_zero();
#    }
  # this is to prevent automatically rounding when MBI's globals are set
  $x->{_m}->{_f} = MB_NEVER_ROUND;
  $x->{_e}->{_f} = MB_NEVER_ROUND;
  # 'forget' that mantissa was rounded via MBI::bround() in MBF's bfround()
  $x->{_m}->{_a} = undef; $x->{_e}->{_a} = undef;
  $x->{_m}->{_p} = undef; $x->{_e}->{_p} = undef;
  $x;					# MBI bnorm is no-op, so dont call it
  } 
 
##############################################################################
# internal calculation routines

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

  # The following all modify their first argument:
  
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
  $x->bdiv($y);			# divide, set $i to quotient
				# return (quo,rem) or quo if scalar

  $x->bmod($y);			# modulus
  $x->bpow($y);			# power of arguments (a**b)
  $x->blsft($y);		# left shift
  $x->brsft($y);		# right shift 
				# return (quo,rem) or quo if scalar
  
  $x->blog($base);		# logarithm of $x, base defaults to e
				# (other bases than e not supported yet)
  
  $x->band($y);			# bit-wise and
  $x->bior($y);			# bit-wise inclusive or
  $x->bxor($y);			# bit-wise exclusive or
  $x->bnot();			# bit-wise not (two's complement)
 
  $x->bsqrt();			# calculate square-root
  $x->bfac();			# factorial of $x (1*2*3*4*..$x)
 
  $x->bround($N); 		# accuracy: preserver $N digits
  $x->bfround($N);		# precision: round to the $Nth digit

  # The following do not modify their arguments:
  bgcd(@values);		# greatest common divisor
  blcm(@values);		# lowest common multiplicator
  
  $x->bstr();			# return string
  $x->bsstr();			# return string in scientific notation
 
  $x->bfloor();			# return integer less or equal than $x
  $x->bceil();			# return integer greater or equal than $x
 
  $x->exponent();		# return exponent as BigInt
  $x->mantissa();		# return mantissa as BigInt
  $x->parts();			# return (mantissa,exponent) as BigInt

  $x->length();			# number of digits (w/o sign and '.')
  ($l,$f) = $x->length();	# number of digits, and length of fraction	

  $x->precision();		# return P of $x (or global, if P of $x undef)
  $x->precision($n);		# set P of $x to $n
  $x->accuracy();		# return A of $x (or global, if A of $x undef)
  $x->accuracy($n);		# set A $x to $n

  Math::BigFloat->precision();	# get/set global P for all BigFloat objects
  Math::BigFloat->accuracy();	# get/set global A for all BigFloat objects

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
while C<bsstr()> (for scientific) gives you the scientific notation.

	Input			bstr()		bsstr()
	'-0'			'0'		'0E1'
   	'  -123 123 123'	'-123123123'	'-123123123E0'
	'00.0123'		'0.0123'	'123E-4'
	'123.45E-2'		'1.2345'	'12345E-4'
	'10E+3'			'10000'		'1E4'

Some routines (C<is_odd()>, C<is_even()>, C<is_zero()>, C<is_one()>,
C<is_nan()>) return true or false, while others (C<bcmp()>, C<bacmp()>)
return either undef, <0, 0 or >0 and are suited for sort.

Actual math is done by using BigInts to represent the mantissa and exponent.
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
C<Math::BigFloat::precision()> digits.

In case the result of one operation has more precision than specified,
it is rounded. The rounding mode taken is either the default mode, or the one
supplied to the operation after the I<scale>:

	$x = Math::BigFloat->new(2);
	Math::BigFloat::precision(5);		# 5 digits max
	$y = $x->copy()->bdiv(3);		# will give 0.66666
	$y = $x->copy()->bdiv(3,6);		# will give 0.666666
	$y = $x->copy()->bdiv(3,6,'odd');	# will give 0.666667
	Math::BigFloat::round_mode('zero');
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

These are effetively no-ops.

=back

All rounding functions take as a second parameter a rounding mode from one of
the following: 'even', 'odd', '+inf', '-inf', 'zero' or 'trunc'.

The default rounding mode is 'even'. By using
C<< Math::BigFloat::round_mode($round_mode); >> you can get and set the default
mode for subsequent rounding. The usage of C<$Math::BigFloat::$round_mode> is
no longer supported.
The second parameter to the round functions then overrides the default
temporarily. 

The C<< as_number() >> function returns a BigInt from a Math::BigFloat. It uses
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

Use the lib, Luke! And see L<Using Math::BigInt::Lite> for more details.

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

If you want to use Math::BigInt's, too, simple add a Math::BigInt B<before>:

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
don't specify it one.

Actually, the lib loading order would be "Bar,Baz,Calc", and then
"Foo,Bar,Baz,Calc", but independend of which lib exists, the result is the
same as trying the latter load alone, except for the fact that Bar or Baz
might be loaded needlessly in an intermidiate step

The old way still works though:

        # 6
        use Math::BigInt lib => 'Bar,Baz';
        use Math::BigFloat;

But B<examples #3 and #4 are recommended> for usage.

=head1 BUGS

=over 2

=item *

The following does not work yet:

	$m = $x->mantissa();
	$e = $x->exponent();
	$y = $m * ( 10 ** $e );
	print "ok\n" if $x == $y;

=item *

There is no fmod() function yet.

=back

=head1 CAVEAT

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
that modifies $x will modify $y, and vice versa.

	$x->bmul(2);
	print "$x, $y\n";	# prints '10, 10'

If you want a true copy of $x, use:
	
	$y = $x->copy();

See also the documentation in L<overload> regarding C<=>.

=item bpow

C<bpow()> now modifies the first argument, unlike the old code which left
it alone and only returned the result. This is to be consistent with
C<badd()> etc. The first will modify $x, the second one won't:

	print bpow($x,$i),"\n"; 	# modify $x
	print $x->bpow($i),"\n"; 	# ditto
	print $x ** $i,"\n";		# leave $x alone 

=back

=head1 LICENSE

This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=head1 AUTHORS

Mark Biggar, overloaded interface by Ilya Zakharevich.
Completely rewritten by Tels http://bloodgate.com in 2001.

=cut
