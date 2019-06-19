#
# "Tax the rat farms." - Lord Vetinari
#

# The following hash values are used:
#   sign : +,-,NaN,+inf,-inf
#   _d   : denominator
#   _n   : numerator (value = _n/_d)
#   _a   : accuracy
#   _p   : precision
# You should not look at the innards of a BigRat - use the methods for this.

package Math::BigRat;

use 5.006;
use strict;
use warnings;

use Carp ();

use Math::BigFloat 1.999718;

our $VERSION = '0.2613';

our @ISA = qw(Math::BigFloat);

our ($accuracy, $precision, $round_mode, $div_scale,
     $upgrade, $downgrade, $_trap_nan, $_trap_inf);

use overload

  # overload key: with_assign

  '+'     =>      sub { $_[0] -> copy() -> badd($_[1]); },

  '-'     =>      sub { my $c = $_[0] -> copy;
                        $_[2] ? $c -> bneg() -> badd( $_[1])
                              : $c -> bsub($_[1]); },

  '*'     =>      sub { $_[0] -> copy() -> bmul($_[1]); },

  '/'     =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bdiv($_[0])
                              : $_[0] -> copy() -> bdiv($_[1]); },


  '%'     =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bmod($_[0])
                              : $_[0] -> copy() -> bmod($_[1]); },

  '**'    =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bpow($_[0])
                              : $_[0] -> copy() -> bpow($_[1]); },

  '<<'    =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> blsft($_[0])
                              : $_[0] -> copy() -> blsft($_[1]); },

  '>>'    =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> brsft($_[0])
                              : $_[0] -> copy() -> brsft($_[1]); },

  # overload key: assign

  '+='    =>      sub { $_[0]->badd($_[1]); },

  '-='    =>      sub { $_[0]->bsub($_[1]); },

  '*='    =>      sub { $_[0]->bmul($_[1]); },

  '/='    =>      sub { scalar $_[0]->bdiv($_[1]); },

  '%='    =>      sub { $_[0]->bmod($_[1]); },

  '**='   =>      sub { $_[0]->bpow($_[1]); },


  '<<='   =>      sub { $_[0]->blsft($_[1]); },

  '>>='   =>      sub { $_[0]->brsft($_[1]); },

#  'x='    =>      sub { },

#  '.='    =>      sub { },

  # overload key: num_comparison

  '<'     =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> blt($_[0])
                              : $_[0] -> blt($_[1]); },

  '<='    =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> ble($_[0])
                              : $_[0] -> ble($_[1]); },

  '>'     =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bgt($_[0])
                              : $_[0] -> bgt($_[1]); },

  '>='    =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bge($_[0])
                              : $_[0] -> bge($_[1]); },

  '=='    =>      sub { $_[0] -> beq($_[1]); },

  '!='    =>      sub { $_[0] -> bne($_[1]); },

  # overload key: 3way_comparison

  '<=>'   =>      sub { my $cmp = $_[0] -> bcmp($_[1]);
                        defined($cmp) && $_[2] ? -$cmp : $cmp; },

  'cmp'   =>      sub { $_[2] ? "$_[1]" cmp $_[0] -> bstr()
                              : $_[0] -> bstr() cmp "$_[1]"; },

  # overload key: str_comparison

#  'lt'     =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bstrlt($_[0])
#                              : $_[0] -> bstrlt($_[1]); },
#
#  'le'    =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bstrle($_[0])
#                              : $_[0] -> bstrle($_[1]); },
#
#  'gt'     =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bstrgt($_[0])
#                              : $_[0] -> bstrgt($_[1]); },
#
#  'ge'    =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bstrge($_[0])
#                              : $_[0] -> bstrge($_[1]); },
#
#  'eq'    =>      sub { $_[0] -> bstreq($_[1]); },
#
#  'ne'    =>      sub { $_[0] -> bstrne($_[1]); },

  # overload key: binary

  '&'     =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> band($_[0])
                              : $_[0] -> copy() -> band($_[1]); },

  '&='    =>      sub { $_[0] -> band($_[1]); },

  '|'     =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bior($_[0])
                              : $_[0] -> copy() -> bior($_[1]); },

  '|='    =>      sub { $_[0] -> bior($_[1]); },

  '^'     =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bxor($_[0])
                              : $_[0] -> copy() -> bxor($_[1]); },

  '^='    =>      sub { $_[0] -> bxor($_[1]); },

#  '&.'    =>      sub { },

#  '&.='   =>      sub { },

#  '|.'    =>      sub { },

#  '|.='   =>      sub { },

#  '^.'    =>      sub { },

#  '^.='   =>      sub { },

  # overload key: unary

  'neg'   =>      sub { $_[0] -> copy() -> bneg(); },

#  '!'     =>      sub { },

  '~'     =>      sub { $_[0] -> copy() -> bnot(); },

#  '~.'    =>      sub { },

  # overload key: mutators

  '++'    =>      sub { $_[0] -> binc() },

  '--'    =>      sub { $_[0] -> bdec() },

  # overload key: func

  'atan2' =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> batan2($_[0])
                              : $_[0] -> copy() -> batan2($_[1]); },

  'cos'   =>      sub { $_[0] -> copy() -> bcos(); },

  'sin'   =>      sub { $_[0] -> copy() -> bsin(); },

  'exp'   =>      sub { $_[0] -> copy() -> bexp($_[1]); },

  'abs'   =>      sub { $_[0] -> copy() -> babs(); },

  'log'   =>      sub { $_[0] -> copy() -> blog(); },

  'sqrt'  =>      sub { $_[0] -> copy() -> bsqrt(); },

  'int'   =>      sub { $_[0] -> copy() -> bint(); },

  # overload key: conversion

  'bool'  =>      sub { $_[0] -> is_zero() ? '' : 1; },

  '""'    =>      sub { $_[0] -> bstr(); },

  '0+'    =>      sub { $_[0] -> numify(); },

  '='     =>      sub { $_[0]->copy(); },

  ;

BEGIN {
    *objectify = \&Math::BigInt::objectify;  # inherit this from BigInt
    *AUTOLOAD  = \&Math::BigFloat::AUTOLOAD; # can't inherit AUTOLOAD
    # We inherit these from BigFloat because currently it is not possible that
    # Math::BigFloat has a different $LIB variable than we, because
    # Math::BigFloat also uses Math::BigInt::config->('lib') (there is always
    # only one library loaded)
    *_e_add = \&Math::BigFloat::_e_add;
    *_e_sub = \&Math::BigFloat::_e_sub;
    *as_int = \&as_number;
    *is_pos = \&is_positive;
    *is_neg = \&is_negative;
}

##############################################################################
# Global constants and flags. Access these only via the accessor methods!

$accuracy   = $precision = undef;
$round_mode = 'even';
$div_scale  = 40;
$upgrade    = undef;
$downgrade  = undef;

# These are internally, and not to be used from the outside at all!

$_trap_nan = 0;                         # are NaNs ok? set w/ config()
$_trap_inf = 0;                         # are infs ok? set w/ config()

# the package we are using for our private parts, defaults to:
# Math::BigInt->config()->{lib}

my $LIB = 'Math::BigInt::Calc';

my $nan   = 'NaN';
#my $class = 'Math::BigRat';

sub isa {
    return 0 if $_[1] =~ /^Math::Big(Int|Float)/;       # we aren't
    UNIVERSAL::isa(@_);
}

##############################################################################

sub new {
    my $proto    = shift;
    my $protoref = ref $proto;
    my $class    = $protoref || $proto;

    # Check the way we are called.

    if ($protoref) {
        Carp::croak("new() is a class method, not an instance method");
    }

    if (@_ < 1) {
        #Carp::carp("Using new() with no argument is deprecated;",
        #           " use bzero() or new(0) instead");
        return $class -> bzero();
    }

    if (@_ > 2) {
        Carp::carp("Superfluous arguments to new() ignored.");
    }

    # Get numerator and denominator. If any of the arguments is undefined,
    # return zero.

    my ($n, $d) = @_;

    if (@_ == 1 && !defined $n ||
        @_ == 2 && (!defined $n || !defined $d))
    {
        #Carp::carp("Use of uninitialized value in new()");
        return $class -> bzero();
    }

    # Initialize a new object.

    my $self = bless {}, $class;

    # One or two input arguments may be given. First handle the numerator $n.

    if (ref($n)) {
        $n = Math::BigFloat -> new($n, undef, undef)
          unless ($n -> isa('Math::BigRat') ||
                  $n -> isa('Math::BigInt') ||
                  $n -> isa('Math::BigFloat'));
    } else {
        if (defined $d) {
            # If the denominator is defined, the numerator is not a string
            # fraction, e.g., "355/113".
            $n = Math::BigFloat -> new($n, undef, undef);
        } else {
            # If the denominator is undefined, the numerator might be a string
            # fraction, e.g., "355/113".
            if ($n =~ m| ^ \s* (\S+) \s* / \s* (\S+) \s* $ |x) {
                $n = Math::BigFloat -> new($1, undef, undef);
                $d = Math::BigFloat -> new($2, undef, undef);
            } else {
                $n = Math::BigFloat -> new($n, undef, undef);
            }
        }
    }

    # At this point $n is an object and $d is either an object or undefined. An
    # undefined $d means that $d was not specified by the caller (not that $d
    # was specified as an undefined value).

    unless (defined $d) {
        #return $n -> copy($n)               if $n -> isa('Math::BigRat');
        return $class -> copy($n)           if $n -> isa('Math::BigRat');
        return $class -> bnan()             if $n -> is_nan();
        return $class -> binf($n -> sign()) if $n -> is_inf();

        if ($n -> isa('Math::BigInt')) {
            $self -> {_n}   = $LIB -> _new($n -> copy() -> babs() -> bstr());
            $self -> {_d}   = $LIB -> _one();
            $self -> {sign} = $n -> sign();
            return $self;
        }

        if ($n -> isa('Math::BigFloat')) {
            my $m = $n -> mantissa() -> babs();
            my $e = $n -> exponent();
            $self -> {_n} = $LIB -> _new($m -> bstr());
            $self -> {_d} = $LIB -> _one();

            if ($e > 0) {
                $self -> {_n} = $LIB -> _lsft($self -> {_n},
                                              $LIB -> _new($e -> bstr()), 10);
            } elsif ($e < 0) {
                $self -> {_d} = $LIB -> _lsft($self -> {_d},
                                              $LIB -> _new(-$e -> bstr()), 10);

                my $gcd = $LIB -> _gcd($LIB -> _copy($self -> {_n}), $self -> {_d});
                if (!$LIB -> _is_one($gcd)) {
                    $self -> {_n} = $LIB -> _div($self->{_n}, $gcd);
                    $self -> {_d} = $LIB -> _div($self->{_d}, $gcd);
                }
            }

            $self -> {sign} = $n -> sign();
            return $self;
        }

        die "I don't know how to handle this";  # should never get here
    }

    # At the point we know that both $n and $d are defined. We know that $n is
    # an object, but $d might still be a scalar. Now handle $d.

    $d = Math::BigFloat -> new($d, undef, undef)
      unless ref($d) && ($d -> isa('Math::BigRat') ||
                         $d -> isa('Math::BigInt') ||
                         $d -> isa('Math::BigFloat'));

    # At this point both $n and $d are objects.

    return $class -> bnan() if $n -> is_nan() || $d -> is_nan();

    # At this point neither $n nor $d is a NaN.

    if ($n -> is_zero()) {
        return $class -> bnan() if $d -> is_zero();     # 0/0 = NaN
        return $class -> bzero();
    }

    return $class -> binf($d -> sign()) if $d -> is_zero();

    # At this point, neither $n nor $d is a NaN or a zero.

    if ($d < 0) {               # make sure denominator is positive
        $n -> bneg();
        $d -> bneg();
    }

    if ($n -> is_inf()) {
        return $class -> bnan() if $d -> is_inf();      # Inf/Inf = NaN
        return $class -> binf($n -> sign());
    }

    # At this point $n is finite.

    return $class -> bzero()            if $d -> is_inf();
    return $class -> binf($d -> sign()) if $d -> is_zero();

    # At this point both $n and $d are finite and non-zero.

    if ($n < 0) {
        $n -> bneg();
        $self -> {sign} = '-';
    } else {
        $self -> {sign} = '+';
    }

    if ($n -> isa('Math::BigRat')) {

        if ($d -> isa('Math::BigRat')) {

            # At this point both $n and $d is a Math::BigRat.

            # p   r    p * s    (p / gcd(p, r)) * (s / gcd(s, q))
            # - / -  = ----- =  ---------------------------------
            # q   s    q * r    (q / gcd(s, q)) * (r / gcd(p, r))

            my $p = $n -> {_n};
            my $q = $n -> {_d};
            my $r = $d -> {_n};
            my $s = $d -> {_d};
            my $gcd_pr = $LIB -> _gcd($LIB -> _copy($p), $r);
            my $gcd_sq = $LIB -> _gcd($LIB -> _copy($s), $q);
            $self -> {_n} = $LIB -> _mul($LIB -> _div($LIB -> _copy($p), $gcd_pr),
                                         $LIB -> _div($LIB -> _copy($s), $gcd_sq));
            $self -> {_d} = $LIB -> _mul($LIB -> _div($LIB -> _copy($q), $gcd_sq),
                                         $LIB -> _div($LIB -> _copy($r), $gcd_pr));

            return $self;       # no need for $self -> bnorm() here
        }

        # At this point, $n is a Math::BigRat and $d is a Math::Big(Int|Float).

        my $p = $n -> {_n};
        my $q = $n -> {_d};
        my $m = $d -> mantissa();
        my $e = $d -> exponent();

        #                   /      p
        #                  |  ------------  if e > 0
        #                  |  q * m * 10^e
        #                  |
        # p                |    p
        # - / (m * 10^e) = |  -----         if e == 0
        # q                |  q * m
        #                  |
        #                  |  p * 10^-e
        #                  |  --------      if e < 0
        #                   \  q * m

        $self -> {_n} = $LIB -> _copy($p);
        $self -> {_d} = $LIB -> _mul($LIB -> _copy($q), $m);
        if ($e > 0) {
            $self -> {_d} = $LIB -> _lsft($self -> {_d}, $e, 10);
        } elsif ($e < 0) {
            $self -> {_n} = $LIB -> _lsft($self -> {_n}, -$e, 10);
        }

        return $self -> bnorm();

    } else {

        if ($d -> isa('Math::BigRat')) {

            # At this point $n is a Math::Big(Int|Float) and $d is a
            # Math::BigRat.

            my $m = $n -> mantissa();
            my $e = $n -> exponent();
            my $p = $d -> {_n};
            my $q = $d -> {_d};

            #                   /  q * m * 10^e
            #                  |   ------------  if e > 0
            #                  |        p
            #                  |
            #              p   |   m * q
            # (m * 10^e) / - = |   -----         if e == 0
            #              q   |     p
            #                  |
            #                  |     q * m
            #                  |   ---------     if e < 0
            #                   \  p * 10^-e

            $self -> {_n} = $LIB -> _mul($LIB -> _copy($q), $m);
            $self -> {_d} = $LIB -> _copy($p);
            if ($e > 0) {
                $self -> {_n} = $LIB -> _lsft($self -> {_n}, $e, 10);
            } elsif ($e < 0) {
                $self -> {_d} = $LIB -> _lsft($self -> {_d}, -$e, 10);
            }
            return $self -> bnorm();

        } else {

            # At this point $n and $d are both a Math::Big(Int|Float)

            my $m1 = $n -> mantissa();
            my $e1 = $n -> exponent();
            my $m2 = $d -> mantissa();
            my $e2 = $d -> exponent();

            #               /
            #              |  m1 * 10^(e1 - e2)
            #              |  -----------------  if e1 > e2
            #              |         m2
            #              |
            # m1 * 10^e1   |  m1
            # ---------- = |  --                 if e1 = e2
            # m2 * 10^e2   |  m2
            #              |
            #              |         m1
            #              |  -----------------  if e1 < e2
            #              |  m2 * 10^(e2 - e1)
            #               \

            $self -> {_n} = $LIB -> _new($m1 -> bstr());
            $self -> {_d} = $LIB -> _new($m2 -> bstr());
            my $ediff = $e1 - $e2;
            if ($ediff > 0) {
                $self -> {_n} = $LIB -> _lsft($self -> {_n},
                                              $LIB -> _new($ediff -> bstr()),
                                              10);
            } elsif ($ediff < 0) {
                $self -> {_d} = $LIB -> _lsft($self -> {_d},
                                              $LIB -> _new(-$ediff -> bstr()),
                                              10);
            }

            return $self -> bnorm();
        }
    }

    return $self;
}

sub copy {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    # If called as a class method, the object to copy is the next argument.

    $self = shift() unless $selfref;

    my $copy = bless {}, $class;

    $copy->{sign} = $self->{sign};
    $copy->{_d} = $LIB->_copy($self->{_d});
    $copy->{_n} = $LIB->_copy($self->{_n});
    $copy->{_a} = $self->{_a} if defined $self->{_a};
    $copy->{_p} = $self->{_p} if defined $self->{_p};

    #($copy, $copy->{_a}, $copy->{_p})
    #  = $copy->_find_round_parameters(@_);

    return $copy;
}

sub bnan {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    $self = bless {}, $class unless $selfref;

    if ($_trap_nan) {
        Carp::croak ("Tried to set a variable to NaN in $class->bnan()");
    }

    $self -> {sign} = $nan;
    $self -> {_n}   = $LIB -> _zero();
    $self -> {_d}   = $LIB -> _one();

    ($self, $self->{_a}, $self->{_p})
      = $self->_find_round_parameters(@_);

    return $self;
}

sub binf {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    $self = bless {}, $class unless $selfref;

    my $sign = shift();
    $sign = defined($sign) && substr($sign, 0, 1) eq '-' ? '-inf' : '+inf';

    if ($_trap_inf) {
        Carp::croak ("Tried to set a variable to +-inf in $class->binf()");
    }

    $self -> {sign} = $sign;
    $self -> {_n}   = $LIB -> _zero();
    $self -> {_d}   = $LIB -> _one();

    ($self, $self->{_a}, $self->{_p})
      = $self->_find_round_parameters(@_);

    return $self;
}

sub bone {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    $self = bless {}, $class unless $selfref;

    my $sign = shift();
    $sign = '+' unless defined($sign) && $sign eq '-';

    $self -> {sign} = $sign;
    $self -> {_n}   = $LIB -> _one();
    $self -> {_d}   = $LIB -> _one();

    ($self, $self->{_a}, $self->{_p})
      = $self->_find_round_parameters(@_);

    return $self;
}

sub bzero {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    $self = bless {}, $class unless $selfref;

    $self -> {sign} = '+';
    $self -> {_n}   = $LIB -> _zero();
    $self -> {_d}   = $LIB -> _one();

    ($self, $self->{_a}, $self->{_p})
      = $self->_find_round_parameters(@_);

    return $self;
}

##############################################################################

sub config {
    # return (later set?) configuration data as hash ref
    my $class = shift() || 'Math::BigRat';

    if (@_ == 1 && ref($_[0]) ne 'HASH') {
        my $cfg = $class->SUPER::config();
        return $cfg->{$_[0]};
    }

    my $cfg = $class->SUPER::config(@_);

    # now we need only to override the ones that are different from our parent
    $cfg->{class} = $class;
    $cfg->{with}  = $LIB;

    $cfg;
}

##############################################################################

sub bstr {
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    if ($x->{sign} !~ /^[+-]$/) {               # inf, NaN etc
        my $s = $x->{sign};
        $s =~ s/^\+//;                          # +inf => inf
        return $s;
    }

    my $s = '';
    $s = $x->{sign} if $x->{sign} ne '+';       # '+3/2' => '3/2'

    return $s . $LIB->_str($x->{_n}) if $LIB->_is_one($x->{_d});
    $s . $LIB->_str($x->{_n}) . '/' . $LIB->_str($x->{_d});
}

sub bsstr {
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    if ($x->{sign} !~ /^[+-]$/) {               # inf, NaN etc
        my $s = $x->{sign};
        $s =~ s/^\+//;                          # +inf => inf
        return $s;
    }

    my $s = '';
    $s = $x->{sign} if $x->{sign} ne '+';       # +3 vs 3
    $s . $LIB->_str($x->{_n}) . '/' . $LIB->_str($x->{_d});
}

sub bnorm {
    # reduce the number to the shortest form
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    # Both parts must be objects of whatever we are using today.
    if (my $c = $LIB->_check($x->{_n})) {
        Carp::croak("n did not pass the self-check ($c) in bnorm()");
    }
    if (my $c = $LIB->_check($x->{_d})) {
        Carp::croak("d did not pass the self-check ($c) in bnorm()");
    }

    # no normalize for NaN, inf etc.
    return $x if $x->{sign} !~ /^[+-]$/;

    # normalize zeros to 0/1
    if ($LIB->_is_zero($x->{_n})) {
        $x->{sign} = '+';                               # never leave a -0
        $x->{_d} = $LIB->_one() unless $LIB->_is_one($x->{_d});
        return $x;
    }

    return $x if $LIB->_is_one($x->{_d});               # no need to reduce

    # Compute the GCD.
    my $gcd = $LIB->_gcd($LIB->_copy($x->{_n}), $x->{_d});
    if (!$LIB->_is_one($gcd)) {
        $x->{_n} = $LIB->_div($x->{_n}, $gcd);
        $x->{_d} = $LIB->_div($x->{_d}, $gcd);
    }

    $x;
}

##############################################################################
# sign manipulation

sub bneg {
    # (BRAT or num_str) return BRAT
    # negate number or make a negated number from string
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return $x if $x->modify('bneg');

    # for +0 do not negate (to have always normalized +0). Does nothing for 'NaN'
    $x->{sign} =~ tr/+-/-+/
      unless ($x->{sign} eq '+' && $LIB->_is_zero($x->{_n}));
    $x;
}

##############################################################################
# special values

sub _bnan {
    # used by parent class bnan() to initialize number to NaN
    my $self = shift;

    if ($_trap_nan) {
        my $class = ref($self);
        # "$self" below will stringify the object, this blows up if $self is a
        # partial object (happens under trap_nan), so fix it beforehand
        $self->{_d} = $LIB->_zero() unless defined $self->{_d};
        $self->{_n} = $LIB->_zero() unless defined $self->{_n};
        Carp::croak ("Tried to set $self to NaN in $class\::_bnan()");
    }
    $self->{_n} = $LIB->_zero();
    $self->{_d} = $LIB->_zero();
}

sub _binf {
    # used by parent class bone() to initialize number to +inf/-inf
    my $self = shift;

    if ($_trap_inf) {
        my $class = ref($self);
        # "$self" below will stringify the object, this blows up if $self is a
        # partial object (happens under trap_nan), so fix it beforehand
        $self->{_d} = $LIB->_zero() unless defined $self->{_d};
        $self->{_n} = $LIB->_zero() unless defined $self->{_n};
        Carp::croak ("Tried to set $self to inf in $class\::_binf()");
    }
    $self->{_n} = $LIB->_zero();
    $self->{_d} = $LIB->_zero();
}

sub _bone {
    # used by parent class bone() to initialize number to +1/-1
    my $self = shift;
    $self->{_n} = $LIB->_one();
    $self->{_d} = $LIB->_one();
}

sub _bzero {
    # used by parent class bzero() to initialize number to 0
    my $self = shift;
    $self->{_n} = $LIB->_zero();
    $self->{_d} = $LIB->_one();
}

##############################################################################
# mul/add/div etc

sub badd {
    # add two rational numbers

    # set up parameters
    my ($class, $x, $y, @r) = (ref($_[0]), @_);
    # objectify is costly, so avoid it
    if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1]))) {
        ($class, $x, $y, @r) = objectify(2, @_);
    }

    # +inf + +inf => +inf, -inf + -inf => -inf
    return $x->binf(substr($x->{sign}, 0, 1))
      if $x->{sign} eq $y->{sign} && $x->{sign} =~ /^[+-]inf$/;

    # +inf + -inf or -inf + +inf => NaN
    return $x->bnan() if ($x->{sign} !~ /^[+-]$/ || $y->{sign} !~ /^[+-]$/);

    #  1   1    gcd(3, 4) = 1    1*3 + 1*4    7
    #  - + -                  = --------- = --
    #  4   3                      4*3       12

    # we do not compute the gcd() here, but simple do:
    #  5   7    5*3 + 7*4   43
    #  - + -  = --------- = --
    #  4   3       4*3      12

    # and bnorm() will then take care of the rest

    # 5 * 3
    $x->{_n} = $LIB->_mul($x->{_n}, $y->{_d});

    # 7 * 4
    my $m = $LIB->_mul($LIB->_copy($y->{_n}), $x->{_d});

    # 5 * 3 + 7 * 4
    ($x->{_n}, $x->{sign}) = _e_add($x->{_n}, $m, $x->{sign}, $y->{sign});

    # 4 * 3
    $x->{_d} = $LIB->_mul($x->{_d}, $y->{_d});

    # normalize result, and possible round
    $x->bnorm()->round(@r);
}

sub bsub {
    # subtract two rational numbers

    # set up parameters
    my ($class, $x, $y, @r) = (ref($_[0]), @_);
    # objectify is costly, so avoid it
    if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1]))) {
        ($class, $x, $y, @r) = objectify(2, @_);
    }

    # flip sign of $x, call badd(), then flip sign of result
    $x->{sign} =~ tr/+-/-+/
      unless $x->{sign} eq '+' && $LIB->_is_zero($x->{_n}); # not -0
    $x->badd($y, @r);           # does norm and round
    $x->{sign} =~ tr/+-/-+/
      unless $x->{sign} eq '+' && $LIB->_is_zero($x->{_n}); # not -0

    $x;
}

sub bmul {
    # multiply two rational numbers

    # set up parameters
    my ($class, $x, $y, @r) = (ref($_[0]), @_);
    # objectify is costly, so avoid it
    if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1]))) {
        ($class, $x, $y, @r) = objectify(2, @_);
    }

    return $x->bnan() if $x->{sign} eq 'NaN' || $y->{sign} eq 'NaN';

    # inf handling
    if ($x->{sign} =~ /^[+-]inf$/ || $y->{sign} =~ /^[+-]inf$/) {
        return $x->bnan() if $x->is_zero() || $y->is_zero();
        # result will always be +-inf:
        # +inf * +/+inf => +inf, -inf * -/-inf => +inf
        # +inf * -/-inf => -inf, -inf * +/+inf => -inf
        return $x->binf() if ($x->{sign} =~ /^\+/ && $y->{sign} =~ /^\+/);
        return $x->binf() if ($x->{sign} =~ /^-/ && $y->{sign} =~ /^-/);
        return $x->binf('-');
    }

    # x == 0  # also: or y == 1 or y == -1
    return wantarray ? ($x, $class->bzero()) : $x if $x -> is_zero();

    if ($y -> is_zero()) {
        $x -> bzero();
        return wantarray ? ($x, $class->bzero()) : $x;
    }

    # According to Knuth, this can be optimized by doing gcd twice (for d
    # and n) and reducing in one step. This saves us a bnorm() at the end.
    #
    # p   s    p * s    (p / gcd(p, r)) * (s / gcd(s, q))
    # - * -  = ----- =  ---------------------------------
    # q   r    q * r    (q / gcd(s, q)) * (r / gcd(p, r))

    my $gcd_pr = $LIB -> _gcd($LIB -> _copy($x->{_n}), $y->{_d});
    my $gcd_sq = $LIB -> _gcd($LIB -> _copy($y->{_n}), $x->{_d});

    $x->{_n} = $LIB -> _mul(scalar $LIB -> _div($x->{_n}, $gcd_pr),
                            scalar $LIB -> _div($LIB -> _copy($y->{_n}),
                                                $gcd_sq));
    $x->{_d} = $LIB -> _mul(scalar $LIB -> _div($x->{_d}, $gcd_sq),
                            scalar $LIB -> _div($LIB -> _copy($y->{_d}),
                                                $gcd_pr));

    # compute new sign
    $x->{sign} = $x->{sign} eq $y->{sign} ? '+' : '-';

    $x->round(@r);
}

sub bdiv {
    # (dividend: BRAT or num_str, divisor: BRAT or num_str) return
    # (BRAT, BRAT) (quo, rem) or BRAT (only rem)

    # set up parameters
    my ($class, $x, $y, @r) = (ref($_[0]), @_);
    # objectify is costly, so avoid it
    if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1]))) {
        ($class, $x, $y, @r) = objectify(2, @_);
    }

    return $x if $x->modify('bdiv');

    my $wantarray = wantarray;  # call only once

    # At least one argument is NaN. This is handled the same way as in
    # Math::BigInt -> bdiv(). See the comments in the code implementing that
    # method.

    if ($x -> is_nan() || $y -> is_nan()) {
        return $wantarray ? ($x -> bnan(), $class -> bnan()) : $x -> bnan();
    }

    # Divide by zero and modulo zero. This is handled the same way as in
    # Math::BigInt -> bdiv(). See the comments in the code implementing that
    # method.

    if ($y -> is_zero()) {
        my ($quo, $rem);
        if ($wantarray) {
            $rem = $x -> copy();
        }
        if ($x -> is_zero()) {
            $quo = $x -> bnan();
        } else {
            $quo = $x -> binf($x -> {sign});
        }
        return $wantarray ? ($quo, $rem) : $quo;
    }

    # Numerator (dividend) is +/-inf. This is handled the same way as in
    # Math::BigInt -> bdiv(). See the comments in the code implementing that
    # method.

    if ($x -> is_inf()) {
        my ($quo, $rem);
        $rem = $class -> bnan() if $wantarray;
        if ($y -> is_inf()) {
            $quo = $x -> bnan();
        } else {
            my $sign = $x -> bcmp(0) == $y -> bcmp(0) ? '+' : '-';
            $quo = $x -> binf($sign);
        }
        return $wantarray ? ($quo, $rem) : $quo;
    }

    # Denominator (divisor) is +/-inf. This is handled the same way as in
    # Math::BigFloat -> bdiv(). See the comments in the code implementing that
    # method.

    if ($y -> is_inf()) {
        my ($quo, $rem);
        if ($wantarray) {
            if ($x -> is_zero() || $x -> bcmp(0) == $y -> bcmp(0)) {
                $rem = $x -> copy();
                $quo = $x -> bzero();
            } else {
                $rem = $class -> binf($y -> {sign});
                $quo = $x -> bone('-');
            }
            return ($quo, $rem);
        } else {
            if ($y -> is_inf()) {
                if ($x -> is_nan() || $x -> is_inf()) {
                    return $x -> bnan();
                } else {
                    return $x -> bzero();
                }
            }
        }
    }

    # At this point, both the numerator and denominator are finite numbers, and
    # the denominator (divisor) is non-zero.

    # x == 0?
    return wantarray ? ($x, $class->bzero()) : $x if $x->is_zero();

    # XXX TODO: list context, upgrade
    # According to Knuth, this can be optimized by doing gcd twice (for d and n)
    # and reducing in one step. This would save us the bnorm() at the end.
    #
    # p   r    p * s    (p / gcd(p, r)) * (s / gcd(s, q))
    # - / -  = ----- =  ---------------------------------
    # q   s    q * r    (q / gcd(s, q)) * (r / gcd(p, r))

    $x->{_n} = $LIB->_mul($x->{_n}, $y->{_d});
    $x->{_d} = $LIB->_mul($x->{_d}, $y->{_n});

    # compute new sign
    $x->{sign} = $x->{sign} eq $y->{sign} ? '+' : '-';

    $x -> bnorm();
    if (wantarray) {
        my $rem = $x -> copy();
        $x -> bfloor();
        $x -> round(@r);
        $rem -> bsub($x -> copy()) -> bmul($y);
        return $x, $rem;
    } else {
        $x -> round(@r);
        return $x;
    }
}

sub bmod {
    # compute "remainder" (in Perl way) of $x / $y

    # set up parameters
    my ($class, $x, $y, @r) = (ref($_[0]), @_);
    # objectify is costly, so avoid it
    if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1]))) {
        ($class, $x, $y, @r) = objectify(2, @_);
    }

    return $x if $x->modify('bmod');

    # At least one argument is NaN. This is handled the same way as in
    # Math::BigInt -> bmod().

    if ($x -> is_nan() || $y -> is_nan()) {
        return $x -> bnan();
    }

    # Modulo zero. This is handled the same way as in Math::BigInt -> bmod().

    if ($y -> is_zero()) {
        return $x;
    }

    # Numerator (dividend) is +/-inf. This is handled the same way as in
    # Math::BigInt -> bmod().

    if ($x -> is_inf()) {
        return $x -> bnan();
    }

    # Denominator (divisor) is +/-inf. This is handled the same way as in
    # Math::BigInt -> bmod().

    if ($y -> is_inf()) {
        if ($x -> is_zero() || $x -> bcmp(0) == $y -> bcmp(0)) {
            return $x;
        } else {
            return $x -> binf($y -> sign());
        }
    }

    # At this point, both the numerator and denominator are finite numbers, and
    # the denominator (divisor) is non-zero.

    return $x if $x->is_zero(); # 0 / 7 = 0, mod 0

    # Compute $x - $y * floor($x/$y). This can probably be optimized by working
    # on a lower level.

    $x -> bsub($x -> copy() -> bdiv($y) -> bfloor() -> bmul($y));
    return $x -> round(@r);
}

##############################################################################
# bdec/binc

sub bdec {
    # decrement value (subtract 1)
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    return $x if $x->{sign} !~ /^[+-]$/; # NaN, inf, -inf

    if ($x->{sign} eq '-') {
        $x->{_n} = $LIB->_add($x->{_n}, $x->{_d}); # -5/2 => -7/2
    } else {
        if ($LIB->_acmp($x->{_n}, $x->{_d}) < 0) # n < d?
        {
            # 1/3 -- => -2/3
            $x->{_n} = $LIB->_sub($LIB->_copy($x->{_d}), $x->{_n});
            $x->{sign} = '-';
        } else {
            $x->{_n} = $LIB->_sub($x->{_n}, $x->{_d}); # 5/2 => 3/2
        }
    }
    $x->bnorm()->round(@r);
}

sub binc {
    # increment value (add 1)
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    return $x if $x->{sign} !~ /^[+-]$/; # NaN, inf, -inf

    if ($x->{sign} eq '-') {
        if ($LIB->_acmp($x->{_n}, $x->{_d}) < 0) {
            # -1/3 ++ => 2/3 (overflow at 0)
            $x->{_n} = $LIB->_sub($LIB->_copy($x->{_d}), $x->{_n});
            $x->{sign} = '+';
        } else {
            $x->{_n} = $LIB->_sub($x->{_n}, $x->{_d}); # -5/2 => -3/2
        }
    } else {
        $x->{_n} = $LIB->_add($x->{_n}, $x->{_d}); # 5/2 => 7/2
    }
    $x->bnorm()->round(@r);
}

##############################################################################
# is_foo methods (the rest is inherited)

sub is_int {
    # return true if arg (BRAT or num_str) is an integer
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return 1 if ($x->{sign} =~ /^[+-]$/) && # NaN and +-inf aren't
      $LIB->_is_one($x->{_d});              # x/y && y != 1 => no integer
    0;
}

sub is_zero {
    # return true if arg (BRAT or num_str) is zero
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return 1 if $x->{sign} eq '+' && $LIB->_is_zero($x->{_n});
    0;
}

sub is_one {
    # return true if arg (BRAT or num_str) is +1 or -1 if signis given
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    my $sign = $_[2] || ''; $sign = '+' if $sign ne '-';
    return 1
      if ($x->{sign} eq $sign && $LIB->_is_one($x->{_n}) && $LIB->_is_one($x->{_d}));
    0;
}

sub is_odd {
    # return true if arg (BFLOAT or num_str) is odd or false if even
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return 1 if ($x->{sign} =~ /^[+-]$/) &&               # NaN & +-inf aren't
      ($LIB->_is_one($x->{_d}) && $LIB->_is_odd($x->{_n})); # x/2 is not, but 3/1
    0;
}

sub is_even {
    # return true if arg (BINT or num_str) is even or false if odd
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return 0 if $x->{sign} !~ /^[+-]$/; # NaN & +-inf aren't
    return 1 if ($LIB->_is_one($x->{_d}) # x/3 is never
                 && $LIB->_is_even($x->{_n})); # but 4/1 is
    0;
}

##############################################################################
# parts() and friends

sub numerator {
    my ($class, $x) = ref($_[0]) ? (ref($_[0]), $_[0]) : objectify(1, @_);

    # NaN, inf, -inf
    return Math::BigInt->new($x->{sign}) if ($x->{sign} !~ /^[+-]$/);

    my $n = Math::BigInt->new($LIB->_str($x->{_n}));
    $n->{sign} = $x->{sign};
    $n;
}

sub denominator {
    my ($class, $x) = ref($_[0]) ? (ref($_[0]), $_[0]) : objectify(1, @_);

    # NaN
    return Math::BigInt->new($x->{sign}) if $x->{sign} eq 'NaN';
    # inf, -inf
    return Math::BigInt->bone() if $x->{sign} !~ /^[+-]$/;

    Math::BigInt->new($LIB->_str($x->{_d}));
}

sub parts {
    my ($class, $x) = ref($_[0]) ? (ref($_[0]), $_[0]) : objectify(1, @_);

    my $c = 'Math::BigInt';

    return ($c->bnan(), $c->bnan()) if $x->{sign} eq 'NaN';
    return ($c->binf(), $c->binf()) if $x->{sign} eq '+inf';
    return ($c->binf('-'), $c->binf()) if $x->{sign} eq '-inf';

    my $n = $c->new($LIB->_str($x->{_n}));
    $n->{sign} = $x->{sign};
    my $d = $c->new($LIB->_str($x->{_d}));
    ($n, $d);
}

sub length {
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return $nan unless $x->is_int();
    $LIB->_len($x->{_n});       # length(-123/1) => length(123)
}

sub digit {
    my ($class, $x, $n) = ref($_[0]) ? (undef, $_[0], $_[1]) : objectify(1, @_);

    return $nan unless $x->is_int();
    $LIB->_digit($x->{_n}, $n || 0); # digit(-123/1, 2) => digit(123, 2)
}

##############################################################################
# special calc routines

sub bceil {
    my ($class, $x) = ref($_[0]) ? (ref($_[0]), $_[0]) : objectify(1, @_);

    return $x if ($x->{sign} !~ /^[+-]$/ ||     # not for NaN, inf
                  $LIB->_is_one($x->{_d}));     # 22/1 => 22, 0/1 => 0

    $x->{_n} = $LIB->_div($x->{_n}, $x->{_d});  # 22/7 => 3/1 w/ truncate
    $x->{_d} = $LIB->_one();                    # d => 1
    $x->{_n} = $LIB->_inc($x->{_n}) if $x->{sign} eq '+';   # +22/7 => 4/1
    $x->{sign} = '+' if $x->{sign} eq '-' && $LIB->_is_zero($x->{_n}); # -0 => 0
    $x;
}

sub bfloor {
    my ($class, $x) = ref($_[0]) ? (ref($_[0]), $_[0]) : objectify(1, @_);

    return $x if ($x->{sign} !~ /^[+-]$/ ||     # not for NaN, inf
                  $LIB->_is_one($x->{_d}));     # 22/1 => 22, 0/1 => 0

    $x->{_n} = $LIB->_div($x->{_n}, $x->{_d});  # 22/7 => 3/1 w/ truncate
    $x->{_d} = $LIB->_one();                    # d => 1
    $x->{_n} = $LIB->_inc($x->{_n}) if $x->{sign} eq '-';   # -22/7 => -4/1
    $x;
}

sub bint {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), $_[0]) : objectify(1, @_);

    return $x if ($x->{sign} !~ /^[+-]$/ ||     # +/-inf or NaN
                  $LIB -> _is_one($x->{_d}));   # already an integer

    $x->{_n} = $LIB->_div($x->{_n}, $x->{_d});  # 22/7 => 3/1 w/ truncate
    $x->{_d} = $LIB->_one();                    # d => 1
    $x->{sign} = '+' if $x->{sign} eq '-' && $LIB -> _is_zero($x->{_n});
    return $x;
}

sub bfac {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # if $x is not an integer
    if (($x->{sign} ne '+') || (!$LIB->_is_one($x->{_d}))) {
        return $x->bnan();
    }

    $x->{_n} = $LIB->_fac($x->{_n});
    # since _d is 1, we don't need to reduce/norm the result
    $x->round(@r);
}

sub bpow {
    # power ($x ** $y)

    # set up parameters
    my ($class, $x, $y, @r) = (ref($_[0]), @_);

    # objectify is costly, so avoid it
    if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1]))) {
        ($class, $x, $y, @r) = objectify(2, @_);
    }

    return $x if $x->{sign} =~ /^[+-]inf$/; # -inf/+inf ** x
    return $x->bnan() if $x->{sign} eq $nan || $y->{sign} eq $nan;
    return $x->bone(@r) if $y->is_zero();
    return $x->round(@r) if $x->is_one() || $y->is_one();

    if ($x->{sign} eq '-' && $LIB->_is_one($x->{_n}) && $LIB->_is_one($x->{_d})) {
        # if $x == -1 and odd/even y => +1/-1
        return $y->is_odd() ? $x->round(@r) : $x->babs()->round(@r);
        # my Casio FX-5500L has a bug here: -1 ** 2 is -1, but -1 * -1 is 1;
    }
    # 1 ** -y => 1 / (1 ** |y|)
    # so do test for negative $y after above's clause

    return $x->round(@r) if $x->is_zero(); # 0**y => 0 (if not y <= 0)

    # shortcut if y == 1/N (is then sqrt() respective broot())
    if ($LIB->_is_one($y->{_n})) {
        return $x->bsqrt(@r) if $LIB->_is_two($y->{_d}); # 1/2 => sqrt
        return $x->broot($LIB->_str($y->{_d}), @r);      # 1/N => root(N)
    }

    # shortcut y/1 (and/or x/1)
    if ($LIB->_is_one($y->{_d})) {
        # shortcut for x/1 and y/1
        if ($LIB->_is_one($x->{_d})) {
            $x->{_n} = $LIB->_pow($x->{_n}, $y->{_n}); # x/1 ** y/1 => (x ** y)/1
            if ($y->{sign} eq '-') {
                # 0.2 ** -3 => 1/(0.2 ** 3)
                ($x->{_n}, $x->{_d}) = ($x->{_d}, $x->{_n}); # swap
            }
            # correct sign; + ** + => +
            if ($x->{sign} eq '-') {
                # - * - => +, - * - * - => -
                $x->{sign} = '+' if $x->{sign} eq '-' && $LIB->_is_even($y->{_n});
            }
            return $x->round(@r);
        }

        # x/z ** y/1
        $x->{_n} = $LIB->_pow($x->{_n}, $y->{_n}); # 5/2 ** y/1 => 5 ** y / 2 ** y
        $x->{_d} = $LIB->_pow($x->{_d}, $y->{_n});
        if ($y->{sign} eq '-') {
            # 0.2 ** -3 => 1/(0.2 ** 3)
            ($x->{_n}, $x->{_d}) = ($x->{_d}, $x->{_n}); # swap
        }
        # correct sign; + ** + => +

        $x->{sign} = '+' if $x->{sign} eq '-' && $LIB->_is_even($y->{_n});
        return $x->round(@r);
    }

    #  print STDERR "# $x $y\n";

    # otherwise:

    #      n/d     n  ______________
    # a/b       =  -\/  (a/b) ** d

    # (a/b) ** n == (a ** n) / (b ** n)
    $LIB->_pow($x->{_n}, $y->{_n});
    $LIB->_pow($x->{_d}, $y->{_n});

    return $x->broot($LIB->_str($y->{_d}), @r); # n/d => root(n)
}

sub blog {
    # Return the logarithm of the operand. If a second operand is defined, that
    # value is used as the base, otherwise the base is assumed to be Euler's
    # constant.

    my ($class, $x, $base, @r);

    # Don't objectify the base, since an undefined base, as in $x->blog() or
    # $x->blog(undef) signals that the base is Euler's number.

    if (!ref($_[0]) && $_[0] =~ /^[A-Za-z]|::/) {
        # E.g., Math::BigFloat->blog(256, 2)
        ($class, $x, $base, @r) =
          defined $_[2] ? objectify(2, @_) : objectify(1, @_);
    } else {
        # E.g., Math::BigFloat::blog(256, 2) or $x->blog(2)
        ($class, $x, $base, @r) =
          defined $_[1] ? objectify(2, @_) : objectify(1, @_);
    }

    return $x if $x->modify('blog');

    # Handle all exception cases and all trivial cases. I have used Wolfram Alpha
    # (http://www.wolframalpha.com) as the reference for these cases.

    return $x -> bnan() if $x -> is_nan();

    if (defined $base) {
        $base = $class -> new($base) unless ref $base;
        if ($base -> is_nan() || $base -> is_one()) {
            return $x -> bnan();
        } elsif ($base -> is_inf() || $base -> is_zero()) {
            return $x -> bnan() if $x -> is_inf() || $x -> is_zero();
            return $x -> bzero();
        } elsif ($base -> is_negative()) {        # -inf < base < 0
            return $x -> bzero() if $x -> is_one(); #     x = 1
            return $x -> bone()  if $x == $base;    #     x = base
            return $x -> bnan();                    #     otherwise
        }
        return $x -> bone() if $x == $base; # 0 < base && 0 < x < inf
    }

    # We now know that the base is either undefined or positive and finite.

    if ($x -> is_inf()) {       # x = +/-inf
        my $sign = defined $base && $base < 1 ? '-' : '+';
        return $x -> binf($sign);
    } elsif ($x -> is_neg()) {  # -inf < x < 0
        return $x -> bnan();
    } elsif ($x -> is_one()) {  # x = 1
        return $x -> bzero();
    } elsif ($x -> is_zero()) { # x = 0
        my $sign = defined $base && $base < 1 ? '+' : '-';
        return $x -> binf($sign);
    }

    # At this point we are done handling all exception cases and trivial cases.

    $base = Math::BigFloat -> new($base) if defined $base;

    my $xn = Math::BigFloat -> new($LIB -> _str($x->{_n}));
    my $xd = Math::BigFloat -> new($LIB -> _str($x->{_d}));

    my $xtmp = Math::BigRat -> new($xn -> bdiv($xd) -> blog($base, @r) -> bsstr());

    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};

    return $x;
}

sub bexp {
    # set up parameters
    my ($class, $x, $y, @r) = (ref($_[0]), @_);

    # objectify is costly, so avoid it
    if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1]))) {
        ($class, $x, $y, @r) = objectify(1, @_);
    }

    return $x->binf(@r)  if $x->{sign} eq '+inf';
    return $x->bzero(@r) if $x->{sign} eq '-inf';

    # we need to limit the accuracy to protect against overflow
    my $fallback = 0;
    my ($scale, @params);
    ($x, @params) = $x->_find_round_parameters(@r);

    # also takes care of the "error in _find_round_parameters?" case
    return $x if $x->{sign} eq 'NaN';

    # no rounding at all, so must use fallback
    if (scalar @params == 0) {
        # simulate old behaviour
        $params[0] = $class->div_scale(); # and round to it as accuracy
        $params[1] = undef;              # P = undef
        $scale = $params[0]+4;           # at least four more for proper round
        $params[2] = $r[2];              # round mode by caller or undef
        $fallback = 1;                   # to clear a/p afterwards
    } else {
        # the 4 below is empirical, and there might be cases where it's not enough...
        $scale = abs($params[0] || $params[1]) + 4; # take whatever is defined
    }

    return $x->bone(@params) if $x->is_zero();

    # See the comments in Math::BigFloat on how this algorithm works.
    # Basically we calculate A and B (where B is faculty(N)) so that A/B = e

    my $x_org = $x->copy();
    if ($scale <= 75) {
        # set $x directly from a cached string form
        $x->{_n} =
          $LIB->_new("90933395208605785401971970164779391644753259799242");
        $x->{_d} =
          $LIB->_new("33452526613163807108170062053440751665152000000000");
        $x->{sign} = '+';
    } else {
        # compute A and B so that e = A / B.

        # After some terms we end up with this, so we use it as a starting point:
        my $A = $LIB->_new("90933395208605785401971970164779391644753259799242");
        my $F = $LIB->_new(42); my $step = 42;

        # Compute how many steps we need to take to get $A and $B sufficiently big
        my $steps = Math::BigFloat::_len_to_steps($scale - 4);
        #    print STDERR "# Doing $steps steps for ", $scale-4, " digits\n";
        while ($step++ <= $steps) {
            # calculate $a * $f + 1
            $A = $LIB->_mul($A, $F);
            $A = $LIB->_inc($A);
            # increment f
            $F = $LIB->_inc($F);
        }
        # compute $B as factorial of $steps (this is faster than doing it manually)
        my $B = $LIB->_fac($LIB->_new($steps));

        #  print "A ", $LIB->_str($A), "\nB ", $LIB->_str($B), "\n";

        $x->{_n} = $A;
        $x->{_d} = $B;
        $x->{sign} = '+';
    }

    # $x contains now an estimate of e, with some surplus digits, so we can round
    if (!$x_org->is_one()) {
        # raise $x to the wanted power and round it in one step:
        $x->bpow($x_org, @params);
    } else {
        # else just round the already computed result
        delete $x->{_a}; delete $x->{_p};
        # shortcut to not run through _find_round_parameters again
        if (defined $params[0]) {
            $x->bround($params[0], $params[2]); # then round accordingly
        } else {
            $x->bfround($params[1], $params[2]); # then round accordingly
        }
    }
    if ($fallback) {
        # clear a/p after round, since user did not request it
        delete $x->{_a}; delete $x->{_p};
    }

    $x;
}

sub bnok {
    # set up parameters
    my ($class, $x, $y, @r) = (ref($_[0]), @_);

    # objectify is costly, so avoid it
    if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1]))) {
        ($class, $x, $y, @r) = objectify(2, @_);
    }

    my $xint = Math::BigInt -> new($x -> bint() -> bsstr());
    my $yint = Math::BigInt -> new($y -> bint() -> bsstr());
    $xint -> bnok($yint);

    $x -> {sign} = $xint -> {sign};
    $x -> {_n}   = $xint -> {_n};
    $x -> {_d}   = $xint -> {_d};

    return $x;
}

sub broot {
    # set up parameters
    my ($class, $x, $y, @r) = (ref($_[0]), @_);
    # objectify is costly, so avoid it
    if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1]))) {
        ($class, $x, $y, @r) = objectify(2, @_);
    }

    # Convert $x into a Math::BigFloat.

    my $xd   = Math::BigFloat -> new($LIB -> _str($x->{_d}));
    my $xflt = Math::BigFloat -> new($LIB -> _str($x->{_n})) -> bdiv($xd);
    $xflt -> {sign} = $x -> {sign};

    # Convert $y into a Math::BigFloat.

    my $yd   = Math::BigFloat -> new($LIB -> _str($y->{_d}));
    my $yflt = Math::BigFloat -> new($LIB -> _str($y->{_n})) -> bdiv($yd);
    $yflt -> {sign} = $y -> {sign};

    # Compute the root and convert back to a Math::BigRat.

    $xflt -> broot($yflt, @r);
    my $xtmp = Math::BigRat -> new($xflt -> bsstr());

    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};

    return $x;
}

sub bmodpow {
    # set up parameters
    my ($class, $x, $y, $m, @r) = (ref($_[0]), @_);
    # objectify is costly, so avoid it
    if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1]))) {
        ($class, $x, $y, $m, @r) = objectify(3, @_);
    }

    # Convert $x, $y, and $m into Math::BigInt objects.

    my $xint = Math::BigInt -> new($x -> copy() -> bint());
    my $yint = Math::BigInt -> new($y -> copy() -> bint());
    my $mint = Math::BigInt -> new($m -> copy() -> bint());

    $xint -> bmodpow($y, $m, @r);
    my $xtmp = Math::BigRat -> new($xint -> bsstr());

    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};
    return $x;
}

sub bmodinv {
    # set up parameters
    my ($class, $x, $y, @r) = (ref($_[0]), @_);
    # objectify is costly, so avoid it
    if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1]))) {
        ($class, $x, $y, @r) = objectify(2, @_);
    }

    # Convert $x and $y into Math::BigInt objects.

    my $xint = Math::BigInt -> new($x -> copy() -> bint());
    my $yint = Math::BigInt -> new($y -> copy() -> bint());

    $xint -> bmodinv($y, @r);
    my $xtmp = Math::BigRat -> new($xint -> bsstr());

    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};
    return $x;
}

sub bsqrt {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    return $x->bnan() if $x->{sign} !~ /^[+]/; # NaN, -inf or < 0
    return $x if $x->{sign} eq '+inf';         # sqrt(inf) == inf
    return $x->round(@r) if $x->is_zero() || $x->is_one();

    local $Math::BigFloat::upgrade = undef;
    local $Math::BigFloat::downgrade = undef;
    local $Math::BigFloat::precision = undef;
    local $Math::BigFloat::accuracy = undef;
    local $Math::BigInt::upgrade = undef;
    local $Math::BigInt::precision = undef;
    local $Math::BigInt::accuracy = undef;

    my $xn = Math::BigFloat -> new($LIB -> _str($x->{_n}));
    my $xd = Math::BigFloat -> new($LIB -> _str($x->{_d}));

    my $xtmp = Math::BigRat -> new($xn -> bdiv($xd) -> bsqrt() -> bsstr());

    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};

    $x->round(@r);
}

sub blsft {
    my ($class, $x, $y, $b, @r) = objectify(2, @_);

    $b = 2 if !defined $b;
    $b = $class -> new($b) unless ref($b) && $b -> isa($class);

    return $x -> bnan() if $x -> is_nan() || $y -> is_nan() || $b -> is_nan();

    # shift by a negative amount?
    return $x -> brsft($y -> copy() -> babs(), $b) if $y -> {sign} =~ /^-/;

    $x -> bmul($b -> bpow($y));
}

sub brsft {
    my ($class, $x, $y, $b, @r) = objectify(2, @_);

    $b = 2 if !defined $b;
    $b = $class -> new($b) unless ref($b) && $b -> isa($class);

    return $x -> bnan() if $x -> is_nan() || $y -> is_nan() || $b -> is_nan();

    # shift by a negative amount?
    return $x -> blsft($y -> copy() -> babs(), $b) if $y -> {sign} =~ /^-/;

    # the following call to bdiv() will return either quotient (scalar context)
    # or quotient and remainder (list context).
    $x -> bdiv($b -> bpow($y));
}

sub band {
    my $x     = shift;
    my $xref  = ref($x);
    my $class = $xref || $x;

    Carp::croak 'band() is an instance method, not a class method' unless $xref;
    Carp::croak 'Not enough arguments for band()' if @_ < 1;

    my $y = shift;
    $y = $class -> new($y) unless ref($y);

    my @r = @_;

    my $xtmp = Math::BigInt -> new($x -> bint());   # to Math::BigInt
    $xtmp -> band($y);
    $xtmp = $class -> new($xtmp);                   # back to Math::BigRat

    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};

    return $x -> round(@r);
}

sub bior {
    my $x     = shift;
    my $xref  = ref($x);
    my $class = $xref || $x;

    Carp::croak 'bior() is an instance method, not a class method' unless $xref;
    Carp::croak 'Not enough arguments for bior()' if @_ < 1;

    my $y = shift;
    $y = $class -> new($y) unless ref($y);

    my @r = @_;

    my $xtmp = Math::BigInt -> new($x -> bint());   # to Math::BigInt
    $xtmp -> bior($y);
    $xtmp = $class -> new($xtmp);                   # back to Math::BigRat

    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};

    return $x -> round(@r);
}

sub bxor {
    my $x     = shift;
    my $xref  = ref($x);
    my $class = $xref || $x;

    Carp::croak 'bxor() is an instance method, not a class method' unless $xref;
    Carp::croak 'Not enough arguments for bxor()' if @_ < 1;

    my $y = shift;
    $y = $class -> new($y) unless ref($y);

    my @r = @_;

    my $xtmp = Math::BigInt -> new($x -> bint());   # to Math::BigInt
    $xtmp -> bxor($y);
    $xtmp = $class -> new($xtmp);                   # back to Math::BigRat

    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};

    return $x -> round(@r);
}

sub bnot {
    my $x     = shift;
    my $xref  = ref($x);
    my $class = $xref || $x;

    Carp::croak 'bnot() is an instance method, not a class method' unless $xref;

    my @r = @_;

    my $xtmp = Math::BigInt -> new($x -> bint());   # to Math::BigInt
    $xtmp -> bnot();
    $xtmp = $class -> new($xtmp);                   # back to Math::BigRat

    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};

    return $x -> round(@r);
}

##############################################################################
# round

sub round {
    $_[0];
}

sub bround {
    $_[0];
}

sub bfround {
    $_[0];
}

##############################################################################
# comparing

sub bcmp {
    # compare two signed numbers

    # set up parameters
    my ($class, $x, $y) = (ref($_[0]), @_);

    # objectify is costly, so avoid it
    if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1]))) {
        ($class, $x, $y) = objectify(2, @_);
    }

    if ($x->{sign} !~ /^[+-]$/ || $y->{sign} !~ /^[+-]$/) {
        # $x is NaN and/or $y is NaN
        return undef if $x->{sign} eq $nan || $y->{sign} eq $nan;
        # $x and $y are both either +inf or -inf
        return 0     if $x->{sign} eq $y->{sign} && $x->{sign} =~ /^[+-]inf$/;
        # $x = +inf and $y < +inf
        return +1    if $x->{sign} eq '+inf';
        # $x = -inf and $y > -inf
        return -1    if $x->{sign} eq '-inf';
        # $x < +inf and $y = +inf
        return -1    if $y->{sign} eq '+inf';
        # $x > -inf and $y = -inf
        return +1;
    }

    # $x >= 0 and $y < 0
    return  1 if $x->{sign} eq '+' && $y->{sign} eq '-';
    # $x < 0 and $y >= 0
    return -1 if $x->{sign} eq '-' && $y->{sign} eq '+';

    # At this point, we know that $x and $y have the same sign.

    # shortcut
    my $xz = $LIB->_is_zero($x->{_n});
    my $yz = $LIB->_is_zero($y->{_n});
    return  0 if $xz && $yz;               # 0 <=> 0
    return -1 if $xz && $y->{sign} eq '+'; # 0 <=> +y
    return  1 if $yz && $x->{sign} eq '+'; # +x <=> 0

    my $t = $LIB->_mul($LIB->_copy($x->{_n}), $y->{_d});
    my $u = $LIB->_mul($LIB->_copy($y->{_n}), $x->{_d});

    my $cmp = $LIB->_acmp($t, $u);     # signs are equal
    $cmp = -$cmp if $x->{sign} eq '-'; # both are '-' => reverse
    $cmp;
}

sub bacmp {
    # compare two numbers (as unsigned)

    # set up parameters
    my ($class, $x, $y) = (ref($_[0]), @_);
    # objectify is costly, so avoid it
    if ((!ref($_[0])) || (ref($_[0]) ne ref($_[1]))) {
        ($class, $x, $y) = objectify(2, @_);
    }

    if (($x->{sign} !~ /^[+-]$/) || ($y->{sign} !~ /^[+-]$/)) {
        # handle +-inf and NaN
        return undef if (($x->{sign} eq $nan) || ($y->{sign} eq $nan));
        return 0 if $x->{sign} =~ /^[+-]inf$/ && $y->{sign} =~ /^[+-]inf$/;
        return 1 if $x->{sign} =~ /^[+-]inf$/ && $y->{sign} !~ /^[+-]inf$/;
        return -1;
    }

    my $t = $LIB->_mul($LIB->_copy($x->{_n}), $y->{_d});
    my $u = $LIB->_mul($LIB->_copy($y->{_n}), $x->{_d});
    $LIB->_acmp($t, $u);        # ignore signs
}

sub beq {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    Carp::croak 'beq() is an instance method, not a class method' unless $selfref;
    Carp::croak 'Wrong number of arguments for beq()' unless @_ == 1;

    my $cmp = $self -> bcmp(shift);
    return defined($cmp) && ! $cmp;
}

sub bne {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    Carp::croak 'bne() is an instance method, not a class method' unless $selfref;
    Carp::croak 'Wrong number of arguments for bne()' unless @_ == 1;

    my $cmp = $self -> bcmp(shift);
    return defined($cmp) && ! $cmp ? '' : 1;
}

sub blt {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    Carp::croak 'blt() is an instance method, not a class method' unless $selfref;
    Carp::croak 'Wrong number of arguments for blt()' unless @_ == 1;

    my $cmp = $self -> bcmp(shift);
    return defined($cmp) && $cmp < 0;
}

sub ble {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    Carp::croak 'ble() is an instance method, not a class method' unless $selfref;
    Carp::croak 'Wrong number of arguments for ble()' unless @_ == 1;

    my $cmp = $self -> bcmp(shift);
    return defined($cmp) && $cmp <= 0;
}

sub bgt {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    Carp::croak 'bgt() is an instance method, not a class method' unless $selfref;
    Carp::croak 'Wrong number of arguments for bgt()' unless @_ == 1;

    my $cmp = $self -> bcmp(shift);
    return defined($cmp) && $cmp > 0;
}

sub bge {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    Carp::croak 'bge() is an instance method, not a class method'
        unless $selfref;
    Carp::croak 'Wrong number of arguments for bge()' unless @_ == 1;

    my $cmp = $self -> bcmp(shift);
    return defined($cmp) && $cmp >= 0;
}

##############################################################################
# output conversion

sub numify {
    # convert 17/8 => float (aka 2.125)
    my ($self, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    # Non-finite number.

    return $x->bstr() if $x->{sign} !~ /^[+-]$/;

    # Finite number.

    my $abs = $LIB->_is_one($x->{_d})
            ? $LIB->_num($x->{_n})
            : Math::BigFloat -> new($LIB->_str($x->{_n}))
                             -> bdiv($LIB->_str($x->{_d}))
                             -> bstr();
    return $x->{sign} eq '-' ? 0 - $abs : 0 + $abs;
}

sub as_number {
    my ($self, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    # NaN, inf etc
    return Math::BigInt->new($x->{sign}) if $x->{sign} !~ /^[+-]$/;

    my $u = Math::BigInt->bzero();
    $u->{value} = $LIB->_div($LIB->_copy($x->{_n}), $x->{_d}); # 22/7 => 3
    $u->bneg if $x->{sign} eq '-'; # no negative zero
    $u;
}

sub as_float {
    # return N/D as Math::BigFloat

    # set up parameters
    my ($class, $x, @r) = (ref($_[0]), @_);
    # objectify is costly, so avoid it
    ($class, $x, @r) = objectify(1, @_) unless ref $_[0];

    # NaN, inf etc
    return Math::BigFloat->new($x->{sign}) if $x->{sign} !~ /^[+-]$/;

    my $xd   = Math::BigFloat -> new($LIB -> _str($x->{_d}));
    my $xflt = Math::BigFloat -> new($LIB -> _str($x->{_n}));
    $xflt -> {sign} = $x -> {sign};
    $xflt -> bdiv($xd, @r);

    return $xflt;
}

sub as_bin {
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return $x unless $x->is_int();

    my $s = $x->{sign};
    $s = '' if $s eq '+';
    $s . $LIB->_as_bin($x->{_n});
}

sub as_hex {
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return $x unless $x->is_int();

    my $s = $x->{sign}; $s = '' if $s eq '+';
    $s . $LIB->_as_hex($x->{_n});
}

sub as_oct {
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return $x unless $x->is_int();

    my $s = $x->{sign}; $s = '' if $s eq '+';
    $s . $LIB->_as_oct($x->{_n});
}

##############################################################################

sub from_hex {
    my $class = shift;

    $class->new(@_);
}

sub from_bin {
    my $class = shift;

    $class->new(@_);
}

sub from_oct {
    my $class = shift;

    my @parts;
    for my $c (@_) {
        push @parts, Math::BigInt->from_oct($c);
    }
    $class->new (@parts);
}

##############################################################################
# import

sub import {
    my $class = shift;
    my $l = scalar @_;
    my $lib = ''; my @a;
    my $try = 'try';

    for (my $i = 0; $i < $l ; $i++) {
        if ($_[$i] eq ':constant') {
            # this rest causes overlord er load to step in
            overload::constant float => sub { $class->new(shift); };
        }
        #    elsif ($_[$i] eq 'upgrade')
        #      {
        #     # this causes upgrading
        #      $upgrade = $_[$i+1];             # or undef to disable
        #      $i++;
        #      }
        elsif ($_[$i] eq 'downgrade') {
            # this causes downgrading
            $downgrade = $_[$i+1]; # or undef to disable
            $i++;
        } elsif ($_[$i] =~ /^(lib|try|only)\z/) {
            $lib = $_[$i+1] || ''; # default Calc
            $try = $1;             # lib, try or only
            $i++;
        } elsif ($_[$i] eq 'with') {
            # this argument is no longer used
            #$LIB = $_[$i+1] || 'Math::BigInt::Calc'; # default Math::BigInt::Calc
            $i++;
        } else {
            push @a, $_[$i];
        }
    }
    require Math::BigInt;

    # let use Math::BigInt lib => 'GMP'; use Math::BigRat; still have GMP
    if ($lib ne '') {
        my @c = split /\s*,\s*/, $lib;
        foreach (@c) {
            $_ =~ tr/a-zA-Z0-9://cd; # limit to sane characters
        }
        $lib = join(",", @c);
    }
    my @import = ('objectify');
    push @import, $try => $lib if $lib ne '';

    # LIB already loaded, so feed it our lib arguments
    Math::BigInt->import(@import);

    $LIB = Math::BigFloat->config()->{lib};

    # register us with LIB to get notified of future lib changes
    Math::BigInt::_register_callback($class, sub { $LIB = $_[0]; });

    # any non :constant stuff is handled by our parent, Exporter (loaded
    # by Math::BigFloat, even if @_ is empty, to give it a chance
    $class->SUPER::import(@a);           # for subclasses
    $class->export_to_level(1, $class, @a); # need this, too
}

1;

__END__

=pod

=head1 NAME

Math::BigRat - Arbitrary big rational numbers

=head1 SYNOPSIS

    use Math::BigRat;

    my $x = Math::BigRat->new('3/7'); $x += '5/9';

    print $x->bstr(), "\n";
    print $x ** 2, "\n";

    my $y = Math::BigRat->new('inf');
    print "$y ", ($y->is_inf ? 'is' : 'is not'), " infinity\n";

    my $z = Math::BigRat->new(144); $z->bsqrt();

=head1 DESCRIPTION

Math::BigRat complements Math::BigInt and Math::BigFloat by providing support
for arbitrary big rational numbers.

=head2 MATH LIBRARY

You can change the underlying module that does the low-level
math operations by using:

    use Math::BigRat try => 'GMP';

Note: This needs Math::BigInt::GMP installed.

The following would first try to find Math::BigInt::Foo, then
Math::BigInt::Bar, and when this also fails, revert to Math::BigInt::Calc:

    use Math::BigRat try => 'Foo,Math::BigInt::Bar';

If you want to get warned when the fallback occurs, replace "try" with "lib":

    use Math::BigRat lib => 'Foo,Math::BigInt::Bar';

If you want the code to die instead, replace "try" with "only":

    use Math::BigRat only => 'Foo,Math::BigInt::Bar';

=head1 METHODS

Any methods not listed here are derived from Math::BigFloat (or
Math::BigInt), so make sure you check these two modules for further
information.

=over

=item new()

    $x = Math::BigRat->new('1/3');

Create a new Math::BigRat object. Input can come in various forms:

    $x = Math::BigRat->new(123);                            # scalars
    $x = Math::BigRat->new('inf');                          # infinity
    $x = Math::BigRat->new('123.3');                        # float
    $x = Math::BigRat->new('1/3');                          # simple string
    $x = Math::BigRat->new('1 / 3');                        # spaced
    $x = Math::BigRat->new('1 / 0.1');                      # w/ floats
    $x = Math::BigRat->new(Math::BigInt->new(3));           # BigInt
    $x = Math::BigRat->new(Math::BigFloat->new('3.1'));     # BigFloat
    $x = Math::BigRat->new(Math::BigInt::Lite->new('2'));   # BigLite

    # You can also give D and N as different objects:
    $x = Math::BigRat->new(
            Math::BigInt->new(-123),
            Math::BigInt->new(7),
         );                      # => -123/7

=item numerator()

    $n = $x->numerator();

Returns a copy of the numerator (the part above the line) as signed BigInt.

=item denominator()

    $d = $x->denominator();

Returns a copy of the denominator (the part under the line) as positive BigInt.

=item parts()

    ($n, $d) = $x->parts();

Return a list consisting of (signed) numerator and (unsigned) denominator as
BigInts.

=item numify()

    my $y = $x->numify();

Returns the object as a scalar. This will lose some data if the object
cannot be represented by a normal Perl scalar (integer or float), so
use L<as_int()|/"as_int()/as_number()"> or L</as_float()> instead.

This routine is automatically used whenever a scalar is required:

    my $x = Math::BigRat->new('3/1');
    @array = (0, 1, 2, 3);
    $y = $array[$x];                # set $y to 3

=item as_int()/as_number()

    $x = Math::BigRat->new('13/7');
    print $x->as_int(), "\n";               # '1'

Returns a copy of the object as BigInt, truncated to an integer.

C<as_number()> is an alias for C<as_int()>.

=item as_float()

    $x = Math::BigRat->new('13/7');
    print $x->as_float(), "\n";             # '1'

    $x = Math::BigRat->new('2/3');
    print $x->as_float(5), "\n";            # '0.66667'

Returns a copy of the object as BigFloat, preserving the
accuracy as wanted, or the default of 40 digits.

This method was added in v0.22 of Math::BigRat (April 2008).

=item as_hex()

    $x = Math::BigRat->new('13');
    print $x->as_hex(), "\n";               # '0xd'

Returns the BigRat as hexadecimal string. Works only for integers.

=item as_bin()

    $x = Math::BigRat->new('13');
    print $x->as_bin(), "\n";               # '0x1101'

Returns the BigRat as binary string. Works only for integers.

=item as_oct()

    $x = Math::BigRat->new('13');
    print $x->as_oct(), "\n";               # '015'

Returns the BigRat as octal string. Works only for integers.

=item from_hex()

    my $h = Math::BigRat->from_hex('0x10');

Create a BigRat from a hexadecimal number in string form.

=item from_oct()

    my $o = Math::BigRat->from_oct('020');

Create a BigRat from an octal number in string form.

=item from_bin()

    my $b = Math::BigRat->from_bin('0b10000000');

Create a BigRat from an binary number in string form.

=item bnan()

    $x = Math::BigRat->bnan();

Creates a new BigRat object representing NaN (Not A Number).
If used on an object, it will set it to NaN:

    $x->bnan();

=item bzero()

    $x = Math::BigRat->bzero();

Creates a new BigRat object representing zero.
If used on an object, it will set it to zero:

    $x->bzero();

=item binf()

    $x = Math::BigRat->binf($sign);

Creates a new BigRat object representing infinity. The optional argument is
either '-' or '+', indicating whether you want infinity or minus infinity.
If used on an object, it will set it to infinity:

    $x->binf();
    $x->binf('-');

=item bone()

    $x = Math::BigRat->bone($sign);

Creates a new BigRat object representing one. The optional argument is
either '-' or '+', indicating whether you want one or minus one.
If used on an object, it will set it to one:

    $x->bone();                 # +1
    $x->bone('-');              # -1

=item length()

    $len = $x->length();

Return the length of $x in digits for integer values.

=item digit()

    print Math::BigRat->new('123/1')->digit(1);     # 1
    print Math::BigRat->new('123/1')->digit(-1);    # 3

Return the N'ths digit from X when X is an integer value.

=item bnorm()

    $x->bnorm();

Reduce the number to the shortest form. This routine is called
automatically whenever it is needed.

=item bfac()

    $x->bfac();

Calculates the factorial of $x. For instance:

    print Math::BigRat->new('3/1')->bfac(), "\n";   # 1*2*3
    print Math::BigRat->new('5/1')->bfac(), "\n";   # 1*2*3*4*5

Works currently only for integers.

=item bround()/round()/bfround()

Are not yet implemented.

=item bmod()

    $x->bmod($y);

Returns $x modulo $y. When $x is finite, and $y is finite and non-zero, the
result is identical to the remainder after floored division (F-division). If,
in addition, both $x and $y are integers, the result is identical to the result
from Perl's % operator.

=item bmodinv()

    $x->bmodinv($mod);          # modular multiplicative inverse

Returns the multiplicative inverse of C<$x> modulo C<$mod>. If

    $y = $x -> copy() -> bmodinv($mod)

then C<$y> is the number closest to zero, and with the same sign as C<$mod>,
satisfying

    ($x * $y) % $mod = 1 % $mod

If C<$x> and C<$y> are non-zero, they must be relative primes, i.e.,
C<bgcd($y, $mod)==1>. 'C<NaN>' is returned when no modular multiplicative
inverse exists.

=item bmodpow()

    $num->bmodpow($exp,$mod);           # modular exponentiation
                                        # ($num**$exp % $mod)

Returns the value of C<$num> taken to the power C<$exp> in the modulus
C<$mod> using binary exponentiation.  C<bmodpow> is far superior to
writing

    $num ** $exp % $mod

because it is much faster - it reduces internal variables into
the modulus whenever possible, so it operates on smaller numbers.

C<bmodpow> also supports negative exponents.

    bmodpow($num, -1, $mod)

is exactly equivalent to

    bmodinv($num, $mod)

=item bneg()

    $x->bneg();

Used to negate the object in-place.

=item is_one()

    print "$x is 1\n" if $x->is_one();

Return true if $x is exactly one, otherwise false.

=item is_zero()

    print "$x is 0\n" if $x->is_zero();

Return true if $x is exactly zero, otherwise false.

=item is_pos()/is_positive()

    print "$x is >= 0\n" if $x->is_positive();

Return true if $x is positive (greater than or equal to zero), otherwise
false. Please note that '+inf' is also positive, while 'NaN' and '-inf' aren't.

C<is_positive()> is an alias for C<is_pos()>.

=item is_neg()/is_negative()

    print "$x is < 0\n" if $x->is_negative();

Return true if $x is negative (smaller than zero), otherwise false. Please
note that '-inf' is also negative, while 'NaN' and '+inf' aren't.

C<is_negative()> is an alias for C<is_neg()>.

=item is_int()

    print "$x is an integer\n" if $x->is_int();

Return true if $x has a denominator of 1 (e.g. no fraction parts), otherwise
false. Please note that '-inf', 'inf' and 'NaN' aren't integer.

=item is_odd()

    print "$x is odd\n" if $x->is_odd();

Return true if $x is odd, otherwise false.

=item is_even()

    print "$x is even\n" if $x->is_even();

Return true if $x is even, otherwise false.

=item bceil()

    $x->bceil();

Set $x to the next bigger integer value (e.g. truncate the number to integer
and then increment it by one).

=item bfloor()

    $x->bfloor();

Truncate $x to an integer value.

=item bint()

    $x->bint();

Round $x towards zero.

=item bsqrt()

    $x->bsqrt();

Calculate the square root of $x.

=item broot()

    $x->broot($n);

Calculate the N'th root of $x.

=item badd()

    $x->badd($y);

Adds $y to $x and returns the result.

=item bmul()

    $x->bmul($y);

Multiplies $y to $x and returns the result.

=item bsub()

    $x->bsub($y);

Subtracts $y from $x and returns the result.

=item bdiv()

    $q = $x->bdiv($y);
    ($q, $r) = $x->bdiv($y);

In scalar context, divides $x by $y and returns the result. In list context,
does floored division (F-division), returning an integer $q and a remainder $r
so that $x = $q * $y + $r. The remainer (modulo) is equal to what is returned
by C<$x->bmod($y)>.

=item bdec()

    $x->bdec();

Decrements $x by 1 and returns the result.

=item binc()

    $x->binc();

Increments $x by 1 and returns the result.

=item copy()

    my $z = $x->copy();

Makes a deep copy of the object.

Please see the documentation in L<Math::BigInt> for further details.

=item bstr()/bsstr()

    my $x = Math::BigRat->new('8/4');
    print $x->bstr(), "\n";             # prints 1/2
    print $x->bsstr(), "\n";            # prints 1/2

Return a string representing this object.

=item bcmp()

    $x->bcmp($y);

Compares $x with $y and takes the sign into account.
Returns -1, 0, 1 or undef.

=item bacmp()

    $x->bacmp($y);

Compares $x with $y while ignoring their sign. Returns -1, 0, 1 or undef.

=item beq()

    $x -> beq($y);

Returns true if and only if $x is equal to $y, and false otherwise.

=item bne()

    $x -> bne($y);

Returns true if and only if $x is not equal to $y, and false otherwise.

=item blt()

    $x -> blt($y);

Returns true if and only if $x is equal to $y, and false otherwise.

=item ble()

    $x -> ble($y);

Returns true if and only if $x is less than or equal to $y, and false
otherwise.

=item bgt()

    $x -> bgt($y);

Returns true if and only if $x is greater than $y, and false otherwise.

=item bge()

    $x -> bge($y);

Returns true if and only if $x is greater than or equal to $y, and false
otherwise.

=item blsft()/brsft()

Used to shift numbers left/right.

Please see the documentation in L<Math::BigInt> for further details.

=item band()

    $x->band($y);               # bitwise and

=item bior()

    $x->bior($y);               # bitwise inclusive or

=item bxor()

    $x->bxor($y);               # bitwise exclusive or

=item bnot()

    $x->bnot();                 # bitwise not (two's complement)

=item bpow()

    $x->bpow($y);

Compute $x ** $y.

Please see the documentation in L<Math::BigInt> for further details.

=item blog()

    $x->blog($base, $accuracy);         # logarithm of x to the base $base

If C<$base> is not defined, Euler's number (e) is used:

    print $x->blog(undef, 100);         # log(x) to 100 digits

=item bexp()

    $x->bexp($accuracy);        # calculate e ** X

Calculates two integers A and B so that A/B is equal to C<e ** $x>, where C<e> is
Euler's number.

This method was added in v0.20 of Math::BigRat (May 2007).

See also C<blog()>.

=item bnok()

    $x->bnok($y);               # x over y (binomial coefficient n over k)

Calculates the binomial coefficient n over k, also called the "choose"
function. The result is equivalent to:

    ( n )      n!
    | - |  = -------
    ( k )    k!(n-k)!

This method was added in v0.20 of Math::BigRat (May 2007).

=item config()

    use Data::Dumper;

    print Dumper ( Math::BigRat->config() );
    print Math::BigRat->config()->{lib}, "\n";

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
    div_scale       RW      Fallback accuracy for div
                            40
    trap_nan        RW      Trap creation of NaN (undef = no)
                            undef
    trap_inf        RW      Trap creation of +inf/-inf (undef = no)
                            undef

By passing a reference to a hash you may set the configuration values. This
works only for values that a marked with a C<RW> above, anything else is
read-only.

=back

=head1 BUGS

Please report any bugs or feature requests to
C<bug-math-bigrat at rt.cpan.org>, or through the web interface at
L<https://rt.cpan.org/Ticket/Create.html?Queue=Math-BigRat>
(requires login).
We will be notified, and then you'll automatically be notified of progress on
your bug as I make changes.

=head1 SUPPORT

You can find documentation for this module with the perldoc command.

    perldoc Math::BigRat

You can also look for information at:

=over 4

=item * RT: CPAN's request tracker

L<https://rt.cpan.org/Public/Dist/Display.html?Name=Math-BigRat>

=item * AnnoCPAN: Annotated CPAN documentation

L<http://annocpan.org/dist/Math-BigRat>

=item * CPAN Ratings

L<http://cpanratings.perl.org/dist/Math-BigRat>

=item * Search CPAN

L<http://search.cpan.org/dist/Math-BigRat/>

=item * CPAN Testers Matrix

L<http://matrix.cpantesters.org/?dist=Math-BigRat>

=item * The Bignum mailing list

=over 4

=item * Post to mailing list

C<bignum at lists.scsys.co.uk>

=item * View mailing list

L<http://lists.scsys.co.uk/pipermail/bignum/>

=item * Subscribe/Unsubscribe

L<http://lists.scsys.co.uk/cgi-bin/mailman/listinfo/bignum>

=back

=back

=head1 LICENSE

This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=head1 SEE ALSO

L<bigrat>, L<Math::BigFloat> and L<Math::BigInt> as well as the backends
L<Math::BigInt::FastCalc>, L<Math::BigInt::GMP>, and L<Math::BigInt::Pari>.

=head1 AUTHORS

=over 4

=item *

Tels L<http://bloodgate.com/> 2001-2009.

=item *

Maintained by Peter John Acklam <pjacklam@online.no> 2011-

=back

=cut
