#
# "Tax the rat farms." - Lord Vetinari
#

# The following hash values are used:

#          sign : "+", "-", "+inf", "-inf", or "NaN"
#            _d : denominator
#            _n : numerator (value = _n/_d)
#      accuracy : accuracy
#     precision : precision

# You should not look at the innards of a BigRat - use the methods for this.

package Math::BigRat;

use 5.006;
use strict;
use warnings;

use Carp            qw< carp croak >;
use Scalar::Util    qw< blessed >;
use Math::BigFloat  qw<>;

our $VERSION = '2.005002';
$VERSION =~ tr/_//d;

require Exporter;
our @ISA = qw< Math::BigFloat >;

use overload

  # overload key: with_assign

  '+'     =>      sub { $_[0] -> copy() -> badd($_[1]); },

  '-'     =>      sub { my $c = $_[0] -> copy;
                        $_[2] ? $c -> bneg() -> badd( $_[1])
                              : $c -> bsub($_[1]); },

  '*'     =>      sub { $_[0] -> copy() -> bmul($_[1]); },

  '/'     =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bfdiv($_[0])
                              : $_[0] -> copy() -> bfdiv($_[1]); },

  '%'     =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bfmod($_[0])
                              : $_[0] -> copy() -> bfmod($_[1]); },

  '**'    =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bpow($_[0])
                              : $_[0] -> copy() -> bpow($_[1]); },

  '<<'    =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bblsft($_[0])
                              : $_[0] -> copy() -> bblsft($_[1]); },

  '>>'    =>      sub { $_[2] ? ref($_[0]) -> new($_[1]) -> bbrsft($_[0])
                              : $_[0] -> copy() -> bbrsft($_[1]); },

  # overload key: assign

  '+='    =>      sub { $_[0] -> badd($_[1]); },

  '-='    =>      sub { $_[0] -> bsub($_[1]); },

  '*='    =>      sub { $_[0] -> bmul($_[1]); },

  '/='    =>      sub { scalar $_[0] -> bfdiv($_[1]); },

  '%='    =>      sub { $_[0] -> bfmod($_[1]); },

  '**='   =>      sub { $_[0] -> bpow($_[1]); },

  '<<='   =>      sub { $_[0] -> bblsft($_[1]); },

  '>>='   =>      sub { $_[0] -> bbrsft($_[1]); },

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
    *objectify = \&Math::BigInt::objectify;

    *AUTOLOAD  = \&Math::BigFloat::AUTOLOAD; # can't inherit AUTOLOAD
    *as_number = \&as_int;
    *is_pos    = \&is_positive;
    *is_neg    = \&is_negative;
}

##############################################################################
# Global constants and flags. Access these only via the accessor methods!

our $accuracy   = undef;
our $precision  = undef;
our $round_mode = 'even';
our $div_scale  = 40;

our $upgrade    = undef;
our $downgrade  = undef;

our $_trap_nan  = 0;            # croak on NaNs?
our $_trap_inf  = 0;            # croak on Infs?

my $nan = 'NaN';                                # constant for easier life

my $LIB = Math::BigInt -> config('lib');        # math backend library

# Has import() been called yet? This variable is needed to make "require" work.

my $IMPORT = 0;

# Compare the following function with @ISA above. This inheritance mess needs a
# clean up. When doing so, also consider the BEGIN block and the AUTOLOAD code.
# Fixme!

sub isa {
    return 0 if $_[1] =~ /^Math::Big(Int|Float)/;       # we aren't
    UNIVERSAL::isa(@_);
}

##############################################################################

sub new {
    # Create a new Math::BigFloat object from a string or another Math::BigInt,
    # Math::BigFloat, or Math::BigRat object. See hash keys documented at top.

    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    # Make "require" work.

    $class -> import() if $IMPORT == 0;

    if (@_ > 2) {
        carp("Superfluous arguments to new() ignored.");
    }

    # Calling new() with no input arguments has been discouraged for more than
    # 10 years, but people apparently still use it, so we still support it.
    # Also, if any of the arguments is undefined, return zero.

    if (@_ == 0 ||
        @_ == 1 && !defined($_[0]) ||
        @_ == 2 && (!defined($_[0]) || !defined($_[1])))
      {
        #carp("Use of uninitialized value in new()");
        return $class -> bzero();
    }

    my @args = @_;

    # Initialize a new object.

    $self = bless {}, $class;

    # Special cases for speed and to avoid infinite recursion. The methods
    # Math::BigInt->as_rat() and Math::BigFloat->as_rat() call
    # Math::BigRat->as_rat() (i.e., this method) with a scalar (non-ref)
    # integer argument.

    if (@args == 1 && !ref($args[0])) {

        # "3", "+3", "-3", "+001_2_3e+4"

        if ($args[0] =~ m{
                             ^
                             \s*

                             # optional sign
                             ( [+-]? )

                             # integer mantissa with optional leading zeros
                             0* ( [1-9] \d* (?: _ \d+ )* | 0 )

                             # optional non-negative exponent
                             (?: [eE] \+? ( \d+ (?: _ \d+ )* ) )?

                             \s*
                             $
                        }x)
        {
            my $sign = $1;
            (my $mant = $2) =~ tr/_//d;
            my $expo = $3;
            $mant .= "0" x $expo if defined($expo) && $mant ne "0";

            $self -> {_n}   = $LIB -> _new($mant);
            $self -> {_d}   = $LIB -> _one();
            $self -> {sign} = $sign eq "-" && $mant ne "0" ? "-" : "+";
            $self -> _dng();
            return $self;
        }

        # "3/5", "+3/5", "-3/5", "+001_2_3e+4 / 05_6e7"

        if ($args[0] =~ m{
                             ^
                             \s*

                             # optional leading sign
                             ( [+-]? )

                             # integer mantissa with optional leading zeros
                             0* ( [1-9] \d* (?: _ \d+ )* | 0 )

                             # optional non-negative exponent
                             (?: [eE] \+? ( \d+ (?: _ \d+ )* ) )?

                             # fraction
                             \s* / \s*

                             # non-zero integer mantissa with optional leading zeros
                             0* ( [1-9] \d* (?: _ \d+ )* )

                             # optional non-negative exponent
                             (?: [eE] \+? ( \d+ (?: _ \d+ )* ) )?

                             \s*
                             $
                        }x)
        {
            my $sign = $1;

            (my $mant1 = $2) =~ tr/_//d;
            my $expo1 = $3;
            $mant1 .= "0" x $expo1 if defined($expo1) && $mant1 ne "0";

            (my $mant2 = $4) =~ tr/_//d;
            my $expo2 = $5;
            $mant2 .= "0" x $expo2 if defined($expo2) && $mant2 ne "0";

            $self -> {_n}   = $LIB -> _new($mant1);
            $self -> {_d}   = $LIB -> _new($mant2);
            $self -> {sign} = $sign eq "-" && $mant1 ne "0" ? "-" : "+";

            my $gcd = $LIB -> _gcd($LIB -> _copy($self -> {_n}),
                                   $self -> {_d});
            unless ($LIB -> _is_one($gcd)) {
                $self -> {_n} = $LIB -> _div($self->{_n}, $gcd);
                $self -> {_d} = $LIB -> _div($self->{_d}, $gcd);
            }

            $self -> _dng() if $self -> is_int();
            return $self;
        }

    }

    # If given exactly one argument which is a string that looks like a
    # fraction, replace this argument with the fraction's numerator and
    # denominator.

    if (@args == 1 && !ref($args[0]) &&
        $args[0] =~ m{ ^ \s* ( \S+ ) \s* / \s* ( \S+ ) \s* $ }x)
    {
        @args = ($1, $2);
    }

    # Now get the numerator and denominator either by calling as_rat() or by
    # letting Math::BigFloat->new() parse the argument as a string.

    my ($n, $d);

    if (@args >= 1) {
        if (ref($args[0]) && $args[0] -> can('as_rat')) {
            $n = $args[0] -> as_rat();
        } else {
            $n = Math::BigFloat -> new($args[0], undef, undef) -> as_rat();
        }
    }

    if (@args >= 2) {
        if (ref($args[1]) && $args[1] -> can('as_rat')) {
            $d = $args[1] -> as_rat();
        } else {
            $d = Math::BigFloat -> new($args[1], undef, undef) -> as_rat();
        }
    }

    $n -> bdiv($d) if defined $d;

    $self -> {sign} = $n -> {sign};
    $self -> {_n}   = $n -> {_n};
    $self -> {_d}   = $n -> {_d};

    $self -> _dng() if ($self -> is_int() ||
                        $self -> is_inf() ||
                        $self -> is_nan());
    return $self;
}

# Create a Math::BigRat from a decimal string. This is an equivalent to
# from_hex(), from_oct(), and from_bin(). It is like new() except that it does
# not accept anything but a string representing a finite decimal number.

sub from_dec {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    # Make "require" work.

    $class -> import() if $IMPORT == 0;

    # Don't modify constant (read-only) objects.

    return $self if $selfref && $self -> modify('from_dec');

    my $str = shift;
    my @r = @_;

    if (my @parts = $class -> _dec_str_to_flt_lib_parts($str)) {

        # If called as a class method, initialize a new object.

        unless ($selfref) {
            $self = bless {}, $class;
            $self -> _init();
        }

        my ($mant_sgn, $mant_abs, $expo_sgn, $expo_abs) = @parts;

        $self->{sign} = $mant_sgn;
        $self->{_n}   = $mant_abs;

        if ($expo_sgn eq "+") {
            $self->{_n} = $LIB -> _lsft($self->{_n}, $expo_abs, 10);
            $self->{_d} = $LIB -> _one();
        } else {
            $self->{_d} = $LIB -> _1ex($expo_abs);
        }

        my $gcd = $LIB -> _gcd($LIB -> _copy($self->{_n}), $self->{_d});
        if (!$LIB -> _is_one($gcd)) {
            $self->{_n} = $LIB -> _div($self->{_n}, $gcd);
            $self->{_d} = $LIB -> _div($self->{_d}, $gcd);
        }

        $self -> _dng() if $self -> is_int();
        return $self;
    }

    return $self -> bnan(@r);
}

sub from_hex {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    # Make "require" work.

    $class -> import() if $IMPORT == 0;

    # Don't modify constant (read-only) objects.

    return $self if $selfref && $self -> modify('from_hex');

    my $str = shift;
    my @r = @_;

    if (my @parts = $class -> _hex_str_to_flt_lib_parts($str)) {

        # If called as a class method, initialize a new object.

        unless ($selfref) {
            $self = bless {}, $class;
            $self -> _init();
        }

        my ($mant_sgn, $mant_abs, $expo_sgn, $expo_abs) = @parts;

        $self->{sign} = $mant_sgn;
        $self->{_n}   = $mant_abs;

        if ($expo_sgn eq "+") {

            # e.g., 345e+2 => 34500/1
            $self->{_n} = $LIB -> _lsft($self->{_n}, $expo_abs, 10);
            $self->{_d} = $LIB -> _one();

        } else {

            # e.g., 345e-2 => 345/100
            $self->{_d} = $LIB -> _1ex($expo_abs);

            # e.g., 345/100 => 69/20
            my $gcd = $LIB -> _gcd($LIB -> _copy($self->{_n}), $self->{_d});
            unless ($LIB -> _is_one($gcd)) {
                $self->{_n} = $LIB -> _div($self->{_n}, $gcd);
                $self->{_d} = $LIB -> _div($self->{_d}, $gcd);
            }
        }

        $self -> _dng() if $self -> is_int();
        return $self;
    }

    return $self -> bnan(@r);
}

sub from_oct {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    # Make "require" work.

    $class -> import() if $IMPORT == 0;

    # Don't modify constant (read-only) objects.

    return $self if $selfref && $self -> modify('from_oct');

    my $str = shift;
    my @r = @_;

    if (my @parts = $class -> _oct_str_to_flt_lib_parts($str)) {

        # If called as a class method, initialize a new object.

        unless ($selfref) {
            $self = bless {}, $class;
            $self -> _init();
        }

        my ($mant_sgn, $mant_abs, $expo_sgn, $expo_abs) = @parts;

        $self->{sign} = $mant_sgn;
        $self->{_n}   = $mant_abs;

        if ($expo_sgn eq "+") {

            # e.g., 345e+2 => 34500/1
            $self->{_n} = $LIB -> _lsft($self->{_n}, $expo_abs, 10);
            $self->{_d} = $LIB -> _one();

        } else {

            # e.g., 345e-2 => 345/100
            $self->{_d} = $LIB -> _1ex($expo_abs);

            # e.g., 345/100 => 69/20
            my $gcd = $LIB -> _gcd($LIB -> _copy($self->{_n}), $self->{_d});
            unless ($LIB -> _is_one($gcd)) {
                $self->{_n} = $LIB -> _div($self->{_n}, $gcd);
                $self->{_d} = $LIB -> _div($self->{_d}, $gcd);
            }
        }

        $self -> _dng() if $self -> is_int();
        return $self;
    }

    return $self -> bnan(@r);
}

sub from_bin {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    # Make "require" work.

    $class -> import() if $IMPORT == 0;

    # Don't modify constant (read-only) objects.

    return $self if $selfref && $self -> modify('from_bin');

    my $str = shift;
    my @r = @_;

    if (my @parts = $class -> _bin_str_to_flt_lib_parts($str)) {

        # If called as a class method, initialize a new object.

        unless ($selfref) {
            $self = bless {}, $class;
            $self -> _init();
        }

        my ($mant_sgn, $mant_abs, $expo_sgn, $expo_abs) = @parts;

        $self->{sign} = $mant_sgn;
        $self->{_n}   = $mant_abs;

        if ($expo_sgn eq "+") {

            # e.g., 345e+2 => 34500/1
            $self->{_n} = $LIB -> _lsft($self->{_n}, $expo_abs, 10);
            $self->{_d} = $LIB -> _one();

        } else {

            # e.g., 345e-2 => 345/100
            $self->{_d} = $LIB -> _1ex($expo_abs);

            # e.g., 345/100 => 69/20
            my $gcd = $LIB -> _gcd($LIB -> _copy($self->{_n}), $self->{_d});
            unless ($LIB -> _is_one($gcd)) {
                $self->{_n} = $LIB -> _div($self->{_n}, $gcd);
                $self->{_d} = $LIB -> _div($self->{_d}, $gcd);
            }
        }

        $self -> _dng() if $self -> is_int();
        return $self;
    }

    return $self -> bnan(@r);
}

sub from_bytes {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    # Make "require" work.

    $class -> import() if $IMPORT == 0;

    # Don't modify constant (read-only) objects.

    return $self if $selfref && $self -> modify('from_bytes');

    my $str = shift;
    my @r = @_;

    # If called as a class method, initialize a new object.

    $self = $class -> bzero(@r) unless $selfref;

    $self -> {sign} = "+";
    $self -> {_n}   = $LIB -> _from_bytes($str);
    $self -> {_d}   = $LIB -> _one();

    $self -> _dng();
    return $self;
}

sub from_ieee754 {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    # Make "require" work.

    $class -> import() if $IMPORT == 0;

    # Don't modify constant (read-only) objects.

    return $self if $selfref && $self -> modify('from_ieee754');

    my $in     = shift;
    my $format = shift;
    my @r      = @_;

    my $tmp = Math::BigFloat -> from_ieee754($in, $format, @r);

    $tmp = $tmp -> as_rat();

    # If called as a class method, initialize a new object.

    $self = $class -> bzero(@r) unless $selfref;
    $self -> {sign} = $tmp -> {sign};
    $self -> {_n}   = $tmp -> {_n};
    $self -> {_d}   = $tmp -> {_d};

    $self -> _dng() if $self -> is_int();
    return $self;
}

sub from_base {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    # Make "require" work.

    $class -> import() if $IMPORT == 0;

    # Don't modify constant (read-only) objects.

    return $self if $selfref && $self -> modify('from_base');

    my ($str, $base, $cs, @r) = @_;     # $cs is the collation sequence

    $base = $class -> new($base) unless ref($base);

    croak("the base must be a finite integer >= 2")
      if $base < 2 || ! $base -> is_int();

    # If called as a class method, initialize a new object.

    $self = $class -> bzero() unless $selfref;

    # If no collating sequence is given, pass some of the conversions to
    # methods optimized for those cases.

    unless (defined $cs) {
        return $self -> from_bin($str, @r) if $base == 2;
        return $self -> from_oct($str, @r) if $base == 8;
        return $self -> from_hex($str, @r) if $base == 16;
        return $self -> from_dec($str, @r) if $base == 10;
    }

    croak("from_base() requires a newer version of the $LIB library.")
      unless $LIB -> can('_from_base');

    $self -> {sign} = '+';
    $self -> {_n}   = $LIB->_from_base($str, $base -> {_n},
                                       defined($cs) ? $cs : ());
    $self -> {_d}   = $LIB->_one();
    $self -> bnorm();

    $self -> _dng();
    return $self;
}

sub bzero {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    # Make "require" work.

    $class -> import() if $IMPORT == 0;

    # Don't modify constant (read-only) objects.

    return $self if $selfref && $self -> modify('bzero');

    # Downgrade?

    my $dng = $class -> downgrade();
    if ($dng && $dng ne $class) {
        return $self -> _dng() -> bzero(@_) if $selfref;
        return $dng -> bzero(@_);
    }

    # Get the rounding parameters, if any.

    my @r = @_;

    # If called as a class method, initialize a new object.

    $self = bless {}, $class unless $selfref;

    $self -> {sign} = '+';
    $self -> {_n}   = $LIB -> _zero();
    $self -> {_d}   = $LIB -> _one();

    # If rounding parameters are given as arguments, use them. If no rounding
    # parameters are given, and if called as a class method initialize the new
    # instance with the class variables.

    #return $self -> round(@r);  # this should work, but doesnt; fixme!

    if (@r) {
        if (@r >= 2 && defined($r[0]) && defined($r[1])) {
            carp "can't specify both accuracy and precision";
            return $self -> bnan();
        }
        $self->{accuracy} = $r[0];
        $self->{precision} = $r[1];
    } else {
        unless($selfref) {
            $self->{accuracy} = $class -> accuracy();
            $self->{precision} = $class -> precision();
        }
    }

    return $self;
}

sub bone {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    # Make "require" work.

    $class -> import() if $IMPORT == 0;

    # Don't modify constant (read-only) objects.

    return $self if $selfref && $self -> modify('bone');

    # Downgrade?

    my $dng = $class -> downgrade();
    if ($dng && $dng ne $class) {
        return $self -> _dng() -> bone(@_) if $selfref;
        return $dng -> bone(@_);
    }

    # Get the sign.

    my $sign = '+';     # default is to return +1
    if (defined($_[0]) && $_[0] =~ /^\s*([+-])\s*$/) {
        $sign = $1;
        shift;
    }

    # Get the rounding parameters, if any.

    my @r = @_;

    # If called as a class method, initialize a new object.

    $self = bless {}, $class unless $selfref;

    $self -> {sign} = $sign;
    $self -> {_n}   = $LIB -> _one();
    $self -> {_d}   = $LIB -> _one();

    # If rounding parameters are given as arguments, use them. If no rounding
    # parameters are given, and if called as a class method initialize the new
    # instance with the class variables.

    #return $self -> round(@r);  # this should work, but doesnt; fixme!

    if (@r) {
        if (@r >= 2 && defined($r[0]) && defined($r[1])) {
            carp "can't specify both accuracy and precision";
            return $self -> bnan();
        }
        $self->{accuracy} = $r[0];
        $self->{precision} = $r[1];
    } else {
        unless($selfref) {
            $self->{accuracy} = $class -> accuracy();
            $self->{precision} = $class -> precision();
        }
    }

    return $self;
}

sub binf {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    {
        no strict 'refs';
        if (${"${class}::_trap_inf"}) {
            croak("Tried to create +-inf in $class->binf()");
        }
    }

    # Make "require" work.

    $class -> import() if $IMPORT == 0;

    # Don't modify constant (read-only) objects.

    return $self if $selfref && $self -> modify('binf');

    # Get the sign.

    my $sign = '+';     # default is to return positive infinity
    if (defined($_[0]) && $_[0] =~ /^\s*([+-])(inf|$)/i) {
        $sign = $1;
        shift;
    }

    # Get the rounding parameters, if any.

    my @r = @_;

    # Downgrade?

    my $dng = $class -> downgrade();
    if ($dng && $dng ne $class) {
        return $self -> _dng() -> binf($sign, @r) if $selfref;
        return $dng -> binf($sign, @r);
    }

    # If called as a class method, initialize a new object.

    $self = bless {}, $class unless $selfref;

    $self -> {sign} = $sign . 'inf';
    $self -> {_n}   = $LIB -> _zero();
    $self -> {_d}   = $LIB -> _one();

    # If rounding parameters are given as arguments, use them. If no rounding
    # parameters are given, and if called as a class method initialize the new
    # instance with the class variables.

    #return $self -> round(@r);  # this should work, but doesnt; fixme!

    if (@r) {
        if (@r >= 2 && defined($r[0]) && defined($r[1])) {
            carp "can't specify both accuracy and precision";
            return $self -> bnan();
        }
        $self->{accuracy} = $r[0];
        $self->{precision} = $r[1];
    } else {
        unless($selfref) {
            $self->{accuracy} = $class -> accuracy();
            $self->{precision} = $class -> precision();
        }
    }

    return $self;
}

sub bnan {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;

    {
        no strict 'refs';
        if (${"${class}::_trap_nan"}) {
            croak("Tried to create NaN in $class->bnan()");
        }
    }

    # Make "require" work.

    $class -> import() if $IMPORT == 0;

    # Don't modify constant (read-only) objects.

    return $self if $selfref && $self -> modify('bnan');

    my $dng = $class -> downgrade();
    if ($dng && $dng ne $class) {
        return $self -> _dng() -> bnan(@_) if $selfref;
        return $dng -> bnan(@_);
    }

    # Get the rounding parameters, if any.

    my @r = @_;

    # If called as a class method, initialize a new object.

    $self = bless {}, $class unless $selfref;

    $self -> {sign} = $nan;
    $self -> {_n}   = $LIB -> _zero();
    $self -> {_d}   = $LIB -> _one();

    # If rounding parameters are given as arguments, use them. If no rounding
    # parameters are given, and if called as a class method initialize the new
    # instance with the class variables.

    #return $self -> round(@r);  # this should work, but doesnt; fixme!

    if (@r) {
        if (@r >= 2 && defined($r[0]) && defined($r[1])) {
            carp "can't specify both accuracy and precision";
            return $self -> bnan();
        }
        $self->{accuracy} = $r[0];
        $self->{precision} = $r[1];
    } else {
        unless($selfref) {
            $self->{accuracy} = $class -> accuracy();
            $self->{precision} = $class -> precision();
        }
    }

    return $self;
}

sub bpi {
    my $self    = shift;
    my $selfref = ref $self;
    my $class   = $selfref || $self;
    my @r       = @_;                   # rounding paramters

    # Make "require" work.

    $class -> import() if $IMPORT == 0;

    # Don't modify constant (read-only) objects.

    return $self if $selfref && $self -> modify('bpi');

    # If called as a class method, initialize a new object.

    $self = bless {}, $class unless $selfref;

    ($self, @r) = $self -> _find_round_parameters(@r);

    # The accuracy, i.e., the number of digits. Pi has one digit before the
    # dot, so a precision of 4 digits is equivalent to an accuracy of 5 digits.

    my $n = defined $r[0] ? $r[0]
          : defined $r[1] ? 1 - $r[1]
          : $self -> div_scale();

    # The algorithm below creates a fraction from a floating point number. The
    # worst case is the number (1 + sqrt(5))/2 (golden ratio), which takes
    # almost 2.4*N iterations to find a fraction that is accurate to N digits,
    # i.e., the relative error is less than 10**(-N).
    #
    # This algorithm might be useful in general, so it should probably be moved
    # out to a method of its own. XXX

    my $max_iter = $n * 2.4;

    my $x = Math::BigFloat -> bpi($n + 10);

    my $tol = $class -> new("1/10") -> bpow("$n") -> bmul($x);

    my $n0 = $class -> bzero();
    my $d0 = $class -> bone();

    my $n1 = $class -> bone();
    my $d1 = $class -> bzero();

    my ($n2, $d2);

    my $xtmp = $x -> copy();

    for (my $iter = 0 ; $iter <= $max_iter ; $iter++) {
        my $t = $xtmp -> copy() -> bint();

        $n2 = $n1 -> copy() -> bmul($t) -> badd($n0);
        $d2 = $d1 -> copy() -> bmul($t) -> badd($d0);

        my $err = $n2 -> copy() -> bdiv($d2) -> bsub($x);
        last if $err -> copy() -> babs() -> ble($tol);

        $xtmp -> bsub($t);
        last if $xtmp -> is_zero();
        $xtmp -> binv();

        ($n1, $n0) = ($n2, $n1);
        ($d1, $d0) = ($d2, $d1);
    }

    my $mbr = $n2 -> bdiv($d2);
    %$self = %$mbr;
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
    $copy->{accuracy} = $self->{accuracy} if defined $self->{accuracy};
    $copy->{precision} = $self->{precision} if defined $self->{precision};

    #($copy, $copy->{accuracy}, $copy->{precision})
    #  = $copy->_find_round_parameters(@_);

    return $copy;
}

sub as_int {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Temporarily disable upgrading and downgrading.

    my $upg = Math::BigInt -> upgrade();
    my $dng = Math::BigInt -> downgrade();
    Math::BigInt -> upgrade(undef);
    Math::BigInt -> downgrade(undef);

    my $y;
    if ($x -> isa("Math::BigInt")) {
        $y = $x -> copy();
    } else {
        if ($x -> is_inf()) {
            $y = Math::BigInt -> binf($x -> sign());
        } elsif ($x -> is_nan()) {
            $y = Math::BigInt -> bnan();
        } else {
            $y = Math::BigInt -> new($x -> copy() -> bint() -> bdstr());
        }

        # Copy the remaining instance variables.

        ($y->{accuracy}, $y->{precision}) = ($x->{accuracy}, $x->{precision});
    }

    $y -> round(@r);

    # Restore upgrading and downgrading.

    Math::BigInt -> upgrade($upg);
    Math::BigInt -> downgrade($dng);

    return $y;
}

sub as_rat {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Temporarily disable upgrading and downgrading.

    require Math::BigRat;
    my $upg = Math::BigRat -> upgrade();
    my $dng = Math::BigRat -> downgrade();
    Math::BigRat -> upgrade(undef);
    Math::BigRat -> downgrade(undef);

    my $y;
    if ($x -> isa("Math::BigRat")) {
        $y = $x -> copy();
    } else {

        if ($x -> is_inf()) {
            $y = Math::BigRat -> binf($x -> sign());
        } elsif ($x -> is_nan()) {
            $y = Math::BigRat -> bnan();
        } else {
            $y = Math::BigRat -> new($x -> bfstr());
        }

        # Copy the remaining instance variables.

        ($y->{accuracy}, $y->{precision}) = ($x->{accuracy}, $x->{precision});
    }

    $y -> round(@r);

    # Restore upgrading and downgrading.

    Math::BigRat -> upgrade($upg);
    Math::BigRat -> downgrade($dng);

    return $y;
}

sub as_float {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Disable upgrading and downgrading.

    require Math::BigFloat;
    my $upg = Math::BigFloat -> upgrade();
    my $dng = Math::BigFloat -> downgrade();
    Math::BigFloat -> upgrade(undef);
    Math::BigFloat -> downgrade(undef);

    my $y;
    if ($x -> isa("Math::BigFloat")) {
        $y = $x -> copy();
    } else {
        if ($x -> is_inf()) {
            $y = Math::BigFloat -> binf($x -> sign());
        } elsif ($x -> is_nan()) {
            $y = Math::BigFloat -> bnan();
        } else {
            if ($x -> isa("Math::BigRat")) {
                if ($x -> is_int()) {
                    $y = Math::BigFloat -> new($x -> bdstr());
                } else {
                    my ($num, $den) = $x -> fparts();
                    my $str = $num -> as_float() -> bdiv($den, @r) -> bdstr();
                    $y = Math::BigFloat -> new($str);
                }
            } else {
                $y = Math::BigFloat -> new($x -> bdstr());
            }
        }

        # Copy the remaining instance variables.

        ($y->{accuracy}, $y->{precision}) = ($x->{accuracy}, $x->{precision});
    }

    $y -> round(@r);

    # Restore upgrading and downgrading.

    Math::BigFloat -> upgrade($upg);
    Math::BigFloat -> downgrade($dng);

    return $y;
}

sub is_zero {
    # return true if arg (BRAT or num_str) is zero
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return 1 if $x->{sign} eq '+' && $LIB->_is_zero($x->{_n});
    return 0;
}

sub is_one {
    # return true if arg (BRAT or num_str) is +1 or -1 if signis given
    my (undef, $x, $sign) = ref($_[0]) ? (undef, @_) : objectify(1, @_);

    if (defined($sign)) {
        croak 'is_one(): sign argument must be "+" or "-"'
          unless $sign eq '+' || $sign eq '-';
    } else {
        $sign = '+';
    }

    return 0 if $x->{sign} ne $sign;
    return 1 if $LIB->_is_one($x->{_n}) && $LIB->_is_one($x->{_d});
    return 0;
}

sub is_odd {
    # return true if arg (BFLOAT or num_str) is odd or false if even
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return 0 unless $x -> is_finite();
    return 1 if $LIB->_is_one($x->{_d}) && $LIB->_is_odd($x->{_n});
    return 0;
}

sub is_even {
    # return true if arg (BINT or num_str) is even or false if odd
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return 0 unless $x -> is_finite();
    return 1 if $LIB->_is_one($x->{_d}) && $LIB->_is_even($x->{_n});
    return 0;
}

sub is_int {
    # return true if arg (BRAT or num_str) is an integer
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return 1 if $x -> is_finite() && $LIB->_is_one($x->{_d});
    return 0;
}

##############################################################################

sub config {
    my $self  = shift;
    my $class = ref($self) || $self || __PACKAGE__;

    # Getter/accessor.

    if (@_ == 1 && ref($_[0]) ne 'HASH') {
        my $param = shift;
        return $class if $param eq 'class';
        return $LIB   if $param eq 'with';
        return $self -> SUPER::config($param);
    }

    # Setter.

    my $cfg = $self -> SUPER::config(@_);

    # We need only to override the ones that are different from our parent.

    unless (ref($self)) {
        $cfg->{class} = $class;
        $cfg->{with} = $LIB;
    }

    $cfg;
}

##############################################################################
# comparing

sub bcmp {
    # compare two signed numbers

    # set up parameters
    my ($class, $x, $y, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_)
                            : objectify(2, @_);

    carp "Rounding is not supported for ", (caller(0))[3], "()" if @r;

    if (!$x -> is_finite() || !$y -> is_finite()) {
        # $x is NaN and/or $y is NaN
        return    if $x -> is_nan() || $y -> is_nan();
        # $x and $y are both either +inf or -inf
        return  0 if $x->{sign} eq $y->{sign} && $x -> is_inf();
        # $x = +inf and $y < +inf
        return +1 if $x -> is_inf("+");
        # $x = -inf and $y > -inf
        return -1 if $x -> is_inf("-");
        # $x < +inf and $y = +inf
        return -1 if $y -> is_inf("+");
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
    my ($class, $x, $y, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_)
                            : objectify(2, @_);

    carp "Rounding is not supported for ", (caller(0))[3], "()" if @r;

    # handle +-inf and NaN
    if (!$x -> is_finite() || !$y -> is_finite()) {
        return    if ($x -> is_nan() || $y -> is_nan());
        return  0 if $x -> is_inf() && $y -> is_inf();
        return  1 if $x -> is_inf() && !$y -> is_inf();
        return -1;
    }

    my $t = $LIB->_mul($LIB->_copy($x->{_n}), $y->{_d});
    my $u = $LIB->_mul($LIB->_copy($y->{_n}), $x->{_d});
    $LIB->_acmp($t, $u);        # ignore signs
}

##############################################################################
# sign manipulation

sub bneg {
    # (BRAT or num_str) return BRAT
    # negate number or make a negated number from string
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bneg');

    # for +0 do not negate (to have always normalized +0). Does nothing for 'NaN'
    $x->{sign} =~ tr/+-/-+/
      unless ($x->{sign} eq '+' && $LIB->_is_zero($x->{_n}));

    $x -> round(@r);
    $x -> _dng() if $x -> is_int() || $x -> is_inf() || $x -> is_nan();
    return $x;
}

sub bnorm {
    # reduce the number to the shortest form
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    # Both parts must be objects of whatever we are using today.
    if (my $c = $LIB->_check($x->{_n})) {
        croak("n did not pass the self-check ($c) in bnorm()");
    }
    if (my $c = $LIB->_check($x->{_d})) {
        croak("d did not pass the self-check ($c) in bnorm()");
    }

    # no normalize for NaN, inf etc.
    if (!$x -> is_finite()) {
        $x -> _dng();
        return $x;
    }

    # normalize zeros to 0/1
    if ($LIB->_is_zero($x->{_n})) {
        $x->{sign} = '+';                               # never leave a -0
        $x->{_d} = $LIB->_one() unless $LIB->_is_one($x->{_d});
        $x -> _dng();
        return $x;
    }

    # n/1
    if ($LIB->_is_one($x->{_d})) {
        $x -> _dng();
        return $x;               # no need to reduce
    }

    # Compute the GCD.
    my $gcd = $LIB->_gcd($LIB->_copy($x->{_n}), $x->{_d});
    if (!$LIB->_is_one($gcd)) {
        $x->{_n} = $LIB->_div($x->{_n}, $gcd);
        $x->{_d} = $LIB->_div($x->{_d}, $gcd);
    }

    $x;
}

sub binc {
    # increment value (add 1)
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('binc');

    if (!$x -> is_finite()) {       # NaN, inf, -inf
        $x -> round(@r);
        $x -> _dng();
        return $x;
    }

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

    $x -> bnorm();      # is this necessary? check! XXX
    $x -> round(@r);
    $x -> _dng() if $x -> is_int() || $x -> is_inf() || $x -> is_nan();
    return $x;
}

sub bdec {
    # decrement value (subtract 1)
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bdec');

    if (!$x -> is_finite()) {       # NaN, inf, -inf
        $x -> round(@r);
        $x -> _dng();
        return $x;
    }

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

    $x -> bnorm();      # is this necessary? check! XXX
    $x -> round(@r);
    $x -> _dng() if $x -> is_int() || $x -> is_inf() || $x -> is_nan();
    return $x;
}

##############################################################################
# mul/add/div etc

sub badd {
    # add two rational numbers

    # set up parameters
    my ($class, $x, $y, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_)
                            : objectify(2, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('badd');

    unless ($x -> is_finite() && $y -> is_finite()) {
        if ($x -> is_nan() || $y -> is_nan()) {
            return $x -> bnan(@r);
        } elsif ($x -> is_inf("+")) {
            return $x -> bnan(@r) if $y -> is_inf("-");
            return $x -> binf("+", @r);
        } elsif ($x -> is_inf("-")) {
            return $x -> bnan(@r) if $y -> is_inf("+");
            return $x -> binf("-", @r);
        } elsif ($y -> is_inf("+")) {
            return $x -> binf("+", @r);
        } elsif ($y -> is_inf("-")) {
            return $x -> binf("-", @r);
        }
    }

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
    ($x->{_n}, $x->{sign}) = $LIB -> _sadd($x->{_n}, $x->{sign}, $m, $y->{sign});

    # 4 * 3
    $x->{_d} = $LIB->_mul($x->{_d}, $y->{_d});

    # normalize result, and possible round
    $x -> bnorm() -> round(@r);
}

sub bsub {
    # subtract two rational numbers

    # set up parameters
    my ($class, $x, $y, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_)
                            : objectify(2, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bsub');

    # flip sign of $x, call badd(), then flip sign of result
    $x->{sign} =~ tr/+-/-+/
      unless $x->{sign} eq '+' && $x -> is_zero();      # not -0
    $x = $x -> badd($y, @r);           # does norm and round
    $x->{sign} =~ tr/+-/-+/
      unless $x->{sign} eq '+' && $x -> is_zero();      # not -0

    $x -> bnorm();
}

sub bmul {
    # multiply two rational numbers

    # set up parameters
    my ($class, $x, $y, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_)
                            : objectify(2, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bmul');

    return $x -> bnan(@r) if $x -> is_nan() || $y -> is_nan();

    # inf handling
    if ($x -> is_inf() || $y -> is_inf()) {
        return $x -> bnan(@r) if $x -> is_zero() || $y -> is_zero();
        # result will always be +-inf:
        # +inf * +/+inf => +inf, -inf * -/-inf => +inf
        # +inf * -/-inf => -inf, -inf * +/+inf => -inf
        return $x -> binf(@r) if $x -> is_positive() && $y -> is_positive();
        return $x -> binf(@r) if $x -> is_negative() && $y -> is_negative();
        return $x -> binf('-', @r);
    }

    return $x -> _upg() -> bmul($y, @r) if $class -> upgrade();

    if ($x -> is_zero() || $y -> is_zero()) {
        return $x -> bzero(@r);
    }

    # According to Knuth, this can be optimized by doing gcd twice (for d
    # and n) and reducing in one step.
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

    $x -> bnorm();      # this is probably redundant; check XXX
    $x -> round(@r);
    $x -> _dng() if $x -> is_int();
    return $x;
}

*bdiv = \&bfdiv;
*bmod = \&bfmod;

sub bfdiv {
    # (dividend: BRAT or num_str, divisor: BRAT or num_str) return
    # (BRAT, BRAT) (quo, rem) or BRAT (only rem)

    # Set up parameters.
    my ($class, $x, $y, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_)
                            : objectify(2, @_);

    ###########################################################################
    # Code for all classes that share the common interface.
    ###########################################################################

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bfdiv');

    my $wantarray = wantarray;          # call only once

    # At least one argument is NaN. This is handled the same way as in
    # Math::BigInt -> bfdiv().

    if ($x -> is_nan() || $y -> is_nan()) {
        return $wantarray ? ($x -> bnan(@r), $class -> bnan(@r))
                          : $x -> bnan(@r);
    }

    # Divide by zero and modulo zero. This is handled the same way as in
    # Math::BigInt -> bfdiv(). See the comments in the code implementing that
    # method.

    if ($y -> is_zero()) {
        my $rem;
        if ($wantarray) {
            $rem = $x -> copy() -> round(@r);
            $rem -> _dng() if $rem -> is_int();
        }
        if ($x -> is_zero()) {
            $x -> bnan(@r);
        } else {
            $x -> binf($x -> {sign}, @r);
        }
        return $wantarray ? ($x, $rem) : $x;
    }

    # Numerator (dividend) is +/-inf. This is handled the same way as in
    # Math::BigInt -> bfdiv(). See the comment in the code for Math::BigInt ->
    # bfdiv() for further details.

    if ($x -> is_inf()) {
        my $rem;
        $rem = $class -> bnan(@r) if $wantarray;
        if ($y -> is_inf()) {
            $x -> bnan(@r);
        } else {
            my $sign = $x -> bcmp(0) == $y -> bcmp(0) ? '+' : '-';
            $x -> binf($sign, @r);
        }
        return $wantarray ? ($x, $rem) : $x;
    }

    # Denominator (divisor) is +/-inf. This is handled the same way as in
    # Math::BigFloat -> bfdiv(). See the comments in the code implementing that
    # method.

    if ($y -> is_inf()) {
        my $rem;
        if ($wantarray) {
            if ($x -> is_zero() || $x -> bcmp(0) == $y -> bcmp(0)) {
                $rem = $x -> copy() -> round(@r);
                $rem -> _dng() if $rem -> is_int();
                $x -> bzero(@r);
            } else {
                $rem = $class -> binf($y -> {sign}, @r);
                $x -> bone('-', @r);
            }
        } else {
            $x -> bzero(@r);
        }
        return $wantarray ? ($x, $rem) : $x;
    }

    # At this point, both the numerator and denominator are finite, non-zero
    # numbers.

    # According to Knuth, this can be optimized by doing gcd twice (for d and n)
    # and reducing in one step. This would save us the bnorm().
    #
    # p   r    p * s    (p / gcd(p, r)) * (s / gcd(s, q))
    # - / -  = ----- =  ---------------------------------
    # q   s    q * r    (q / gcd(s, q)) * (r / gcd(p, r))

    $x->{_n} = $LIB->_mul($x->{_n}, $y->{_d});
    $x->{_d} = $LIB->_mul($x->{_d}, $y->{_n});

    # compute new sign
    $x->{sign} = $x->{sign} eq $y->{sign} ? '+' : '-';

    $x -> bnorm();
    if ($wantarray) {
        my $rem = $x -> copy();
        $x -> bfloor();
        $x -> round(@r);
        $rem -> bsub($x -> copy()) -> bmul($y);
        $x -> _dng() if $x -> is_int();
        $rem -> _dng() if $rem -> is_int();
        return $x, $rem;
    }

    $x -> _dng() if $x -> is_int();
    return $x;
}

sub bfmod {
    # This is the remainder after floored division.

    # Set up parameters.
    my ($class, $x, $y, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_)
                            : objectify(2, @_);

    ###########################################################################
    # Code for all classes that share the common interface.
    ###########################################################################

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bfmod');

    # At least one argument is NaN. This is handled the same way as in
    # Math::BigInt -> bfmod().

    if ($x -> is_nan() || $y -> is_nan()) {
        return $x -> bnan();
    }

    # Modulo zero. This is handled the same way as in Math::BigInt -> bfmod().

    if ($y -> is_zero()) {
        return $x -> round();
    }

    # Numerator (dividend) is +/-inf. This is handled the same way as in
    # Math::BigInt -> bfmod().

    if ($x -> is_inf()) {
        return $x -> bnan();
    }

    # Denominator (divisor) is +/-inf. This is handled the same way as in
    # Math::BigInt -> bfmod().

    if ($y -> is_inf()) {
        if ($x -> is_zero() || $x -> bcmp(0) == $y -> bcmp(0)) {
            $x -> _dng() if $x -> is_int();
            return $x;
        } else {
            return $x -> binf($y -> sign());
        }
    }

    # At this point, both the numerator and denominator are finite numbers, and
    # the denominator (divisor) is non-zero.

    if ($x -> is_zero()) {        # 0 / 7 = 0, mod 0
        return $x -> bzero();
    }

    # Compute $x - $y * floor($x / $y). This can be optimized by working on the
    # library thingies directly. XXX

    $x -> bsub($x -> copy() -> bfdiv($y) -> bfloor() -> bmul($y));
    return $x -> round(@r);
}

sub btdiv {
    # (dividend: BRAT or num_str, divisor: BRAT or num_str) return
    # (BRAT, BRAT) (quo, rem) or BRAT (only rem)

    # Set up parameters.
    my ($class, $x, $y, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_)
                            : objectify(2, @_);

    ###########################################################################
    # Code for all classes that share the common interface.
    ###########################################################################

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('btdiv');

    my $wantarray = wantarray;          # call only once

    # At least one argument is NaN. This is handled the same way as in
    # Math::BigInt -> btdiv().

    if ($x -> is_nan() || $y -> is_nan()) {
        return $wantarray ? ($x -> bnan(@r), $class -> bnan(@r))
                          : $x -> bnan(@r);
    }

    # Divide by zero and modulo zero. This is handled the same way as in
    # Math::BigInt -> btdiv(). See the comments in the code implementing that
    # method.

    if ($y -> is_zero()) {
        my $rem;
        if ($wantarray) {
            $rem = $x -> copy() -> round(@r);
            $rem -> _dng() if $rem -> is_int();
        }
        if ($x -> is_zero()) {
            $x -> bnan(@r);
        } else {
            $x -> binf($x -> {sign}, @r);
        }
        return $wantarray ? ($x, $rem) : $x;
    }

    # Numerator (dividend) is +/-inf. This is handled the same way as in
    # Math::BigInt -> btdiv(). See the comment in the code for Math::BigInt ->
    # btdiv() for further details.

    if ($x -> is_inf()) {
        my $rem;
        $rem = $class -> bnan(@r) if $wantarray;
        if ($y -> is_inf()) {
            $x -> bnan(@r);
        } else {
            my $sign = $x -> bcmp(0) == $y -> bcmp(0) ? '+' : '-';
            $x -> binf($sign, @r);
        }
        return $wantarray ? ($x, $rem) : $x;
    }

    # Denominator (divisor) is +/-inf. This is handled the same way as in
    # Math::BigFloat -> btdiv(). See the comments in the code implementing that
    # method.

    if ($y -> is_inf()) {
        my $rem;
        if ($wantarray) {
            $rem = $x -> copy();
            $rem -> _dng() if $rem -> is_int();
            $x -> bzero();
            return $x, $rem;
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

    if ($x -> is_zero()) {
        $x -> round(@r);
        $x -> _dng() if $x -> is_int();
        if ($wantarray) {
            my $rem = $class -> bzero(@r);
            return $x, $rem;
        }
        return $x;
    }

    # At this point, both the numerator and denominator are finite, non-zero
    # numbers.

    # According to Knuth, this can be optimized by doing gcd twice (for d and n)
    # and reducing in one step. This would save us the bnorm().
    #
    # p   r    p * s    (p / gcd(p, r)) * (s / gcd(s, q))
    # - / -  = ----- =  ---------------------------------
    # q   s    q * r    (q / gcd(s, q)) * (r / gcd(p, r))

    $x->{_n} = $LIB->_mul($x->{_n}, $y->{_d});
    $x->{_d} = $LIB->_mul($x->{_d}, $y->{_n});

    # compute new sign
    $x->{sign} = $x->{sign} eq $y->{sign} ? '+' : '-';

    $x -> bnorm();
    if ($wantarray) {
        my $rem = $x -> copy();
        $x -> bint();
        $x -> round(@r);
        $rem -> bsub($x -> copy()) -> bmul($y);
        $x -> _dng() if $x -> is_int();
        $rem -> _dng() if $rem -> is_int();
        return $x, $rem;
    }

    $x -> _dng() if $x -> is_int();
    return $x;
}

sub btmod {
    # This is the remainder after floored division.

    # Set up parameters.
    my ($class, $x, $y, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_)
                            : objectify(2, @_);

    ###########################################################################
    # Code for all classes that share the common interface.
    ###########################################################################

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('btmod');

    # At least one argument is NaN. This is handled the same way as in
    # Math::BigInt -> btmod().

    if ($x -> is_nan() || $y -> is_nan()) {
        return $x -> bnan();
    }

    # Modulo zero. This is handled the same way as in Math::BigInt -> btmod().

    if ($y -> is_zero()) {
        return $x -> round();
    }

    # Numerator (dividend) is +/-inf. This is handled the same way as in
    # Math::BigInt -> btmod().

    if ($x -> is_inf()) {
        return $x -> bnan();
    }

    # Denominator (divisor) is +/-inf. This is handled the same way as in
    # Math::BigInt -> btmod().

    if ($y -> is_inf()) {
        $x -> _dng() if $x -> is_int();
        return $x;
    }

    # At this point, both the numerator and denominator are finite numbers, and
    # the denominator (divisor) is non-zero.

    if ($x -> is_zero()) {        # 0 / 7 = 0, mod 0
        return $x -> bzero();
    }

    # Compute $x - $y * int($x / $y).
    #
    # p     r   (p * s / gcd(q, s)) mod (r * q / gcd(q, s))
    # - mod - = -------------------------------------------
    # q     s                q * s / gcd(q, s)
    #
    #   u mod v         u = p * (s / gcd(q, s))
    # = -------  where  v = r * (q / gcd(q, s))
    #      w            w = q * (s / gcd(q, s))

    my $p = $x -> {_n};
    my $q = $x -> {_d};
    my $r = $y -> {_n};
    my $s = $y -> {_d};

    my $gcd_qs      = $LIB -> _gcd($LIB -> _copy($q), $s);
    my $s_by_gcd_qs = $LIB -> _div($LIB -> _copy($s), $gcd_qs);
    my $q_by_gcd_qs = $LIB -> _div($LIB -> _copy($q), $gcd_qs);

    my $u = $LIB -> _mul($LIB -> _copy($p), $s_by_gcd_qs);
    my $v = $LIB -> _mul($LIB -> _copy($r), $q_by_gcd_qs);
    my $w = $LIB -> _mul($LIB -> _copy($q), $s_by_gcd_qs);

    $x->{_n} = $LIB -> _mod($u, $v);
    $x->{_d} = $w;

    $x -> bnorm();
    return $x -> round(@r);
}

sub binv {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), $_[0]) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('binv');

    return $x -> round(@r)     if $x -> is_nan();
    return $x -> bzero(@r)     if $x -> is_inf();
    return $x -> binf("+", @r) if $x -> is_zero();

    ($x -> {_n}, $x -> {_d}) = ($x -> {_d}, $x -> {_n});

    $x -> round(@r);
    $x -> _dng() if $x -> is_int() || $x -> is_inf() || $x -> is_nan();
    return $x;
}

sub bsqrt {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bsqrt');

    return $x -> bnan() if $x->{sign} !~ /^[+]/; # NaN, -inf or < 0
    return $x if $x -> is_inf("+");         # sqrt(inf) == inf
    return $x -> round(@r) if $x -> is_zero() || $x -> is_one();

    my $n = $x -> {_n};
    my $d = $x -> {_d};

    # Look for an exact solution. For the numerator and the denominator, take
    # the square root and square it and see if we got the original value. If we
    # did, for both the numerator and the denominator, we have an exact
    # solution.

    {
        my $nsqrt = $LIB -> _sqrt($LIB -> _copy($n));
        my $n2    = $LIB -> _mul($LIB -> _copy($nsqrt), $nsqrt);
        if ($LIB -> _acmp($n, $n2) == 0) {
            my $dsqrt = $LIB -> _sqrt($LIB -> _copy($d));
            my $d2    = $LIB -> _mul($LIB -> _copy($dsqrt), $dsqrt);
            if ($LIB -> _acmp($d, $d2) == 0) {
                $x -> {_n} = $nsqrt;
                $x -> {_d} = $dsqrt;
                return $x -> round(@r);
            }
        }
    }

    local $Math::BigFloat::upgrade   = undef;
    local $Math::BigFloat::downgrade = undef;
    local $Math::BigFloat::precision = undef;
    local $Math::BigFloat::accuracy  = undef;
    local $Math::BigInt::upgrade     = undef;
    local $Math::BigInt::precision   = undef;
    local $Math::BigInt::accuracy    = undef;

    my $xn = Math::BigFloat -> new($LIB -> _str($n));
    my $xd = Math::BigFloat -> new($LIB -> _str($d));

    my $xtmp = Math::BigRat -> new($xn -> bfdiv($xd) -> bsqrt() -> bfstr());

    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};

    $x -> round(@r);
}

sub bpow {
    # power ($x ** $y)

    # Set up parameters.
    my ($class, $x, $y, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_)
                            : objectify(2, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bpow');

    # $x and/or $y is a NaN
    return $x -> bnan() if $x -> is_nan() || $y -> is_nan();

    # $x and/or $y is a +/-Inf
    if ($x -> is_inf("-")) {
        return $x -> bzero()   if $y -> is_negative();
        return $x -> bnan()    if $y -> is_zero();
        return $x            if $y -> is_odd();
        return $x -> bneg();
    } elsif ($x -> is_inf("+")) {
        return $x -> bzero()   if $y -> is_negative();
        return $x -> bnan()    if $y -> is_zero();
        return $x;
    } elsif ($y -> is_inf("-")) {
        return $x -> bnan()    if $x -> is_one("-");
        return $x -> binf("+") if $x > -1 && $x < 1;
        return $x -> bone()    if $x -> is_one("+");
        return $x -> bzero();
    } elsif ($y -> is_inf("+")) {
        return $x -> bnan()    if $x -> is_one("-");
        return $x -> bzero()   if $x > -1 && $x < 1;
        return $x -> bone()    if $x -> is_one("+");
        return $x -> binf("+");
    }

    if ($x -> is_zero()) {
        return $x -> bone() if $y -> is_zero();
        return $x -> binf() if $y -> is_negative();
        return $x;
    }

    # We don't support complex numbers, so upgrade or return NaN.

    if ($x -> is_negative() && !$y -> is_int()) {
        return $x -> _upg() -> bpow($y, @r) if $class -> upgrade();
        return $x -> bnan();
    }

    if ($x -> is_one("+") || $y -> is_one()) {
        return $x;
    }

    if ($x -> is_one("-")) {
        return $x if $y -> is_odd();
        return $x -> bneg();
    }

    # (a/b)^-(c/d) = (b/a)^(c/d)
    ($x->{_n}, $x->{_d}) = ($x->{_d}, $x->{_n}) if $y -> is_negative();

    unless ($LIB->_is_one($y->{_n})) {
        $x->{_n} = $LIB->_pow($x->{_n}, $y->{_n});
        $x->{_d} = $LIB->_pow($x->{_d}, $y->{_n});
        $x->{sign} = '+' if $x->{sign} eq '-' && $LIB->_is_even($y->{_n});
    }

    unless ($LIB->_is_one($y->{_d})) {
        return $x -> bsqrt(@r) if $LIB->_is_two($y->{_d}); # 1/2 => sqrt
        return $x -> broot($LIB->_str($y->{_d}), @r);      # 1/N => root(N)
    }

    return $x -> round(@r);
}

sub broot {
    # set up parameters
    my ($class, $x, $y, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_)
                            : objectify(2, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('broot');

    # Convert $x into a Math::BigFloat.

    my $xd   = Math::BigFloat -> new($LIB -> _str($x->{_d}));
    my $xflt = Math::BigFloat -> new($LIB -> _str($x->{_n})) -> bfdiv($xd);
    $xflt -> {sign} = $x -> {sign};

    # Convert $y into a Math::BigFloat.

    my $yd   = Math::BigFloat -> new($LIB -> _str($y->{_d}));
    my $yflt = Math::BigFloat -> new($LIB -> _str($y->{_n})) -> bfdiv($yd);
    $yflt -> {sign} = $y -> {sign};

    # Compute the root and convert back to a Math::BigRat.

    $xflt -> broot($yflt, @r);
    my $xtmp = Math::BigRat -> new($xflt -> bfstr());

    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};

    return $x;
}

sub bmuladd {
    # multiply two numbers and then add the third to the result
    # (BINT or num_str, BINT or num_str, BINT or num_str) return BINT

    # set up parameters
    my ($class, $x, $y, $z, @r)
      = ref($_[0]) && ref($_[0]) eq ref($_[1]) && ref($_[1]) eq ref($_[2])
      ? (ref($_[0]), @_)
      : objectify(3, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bmuladd');

    # At least one of x, y, and z is a NaN

    return $x -> bnan(@r) if ($x -> is_nan() ||
                              $y -> is_nan() ||
                              $z -> is_nan());

    # At least one of x, y, and z is an Inf

    if ($x -> is_inf("-")) {

        if ($y -> is_neg()) {                   # x = -inf, y < 0
            if ($z -> is_inf("-")) {
                return $x -> bnan(@r);
            } else {
                return $x -> binf("+", @r);
            }
        } elsif ($y -> is_zero()) {             # x = -inf, y = 0
            return $x -> bnan(@r);
        } else {                                # x = -inf, y > 0
            if ($z -> is_inf("+")) {
                return $x -> bnan(@r);
            } else {
                return $x -> binf("-", @r);
            }
        }

    } elsif ($x -> is_inf("+")) {

        if ($y -> is_neg()) {                   # x = +inf, y < 0
            if ($z -> is_inf("+")) {
                return $x -> bnan(@r);
            } else {
                return $x -> binf("-", @r);
            }
        } elsif ($y -> is_zero()) {             # x = +inf, y = 0
            return $x -> bnan(@r);
        } else {                                # x = +inf, y > 0
            if ($z -> is_inf("-")) {
                return $x -> bnan(@r);
            } else {
                return $x -> binf("+", @r);
            }
        }

    } elsif ($x -> is_neg()) {

        if ($y -> is_inf("-")) {                # -inf < x < 0, y = -inf
            if ($z -> is_inf("-")) {
                return $x -> bnan(@r);
            } else {
                return $x -> binf("+", @r);
            }
        } elsif ($y -> is_inf("+")) {           # -inf < x < 0, y = +inf
            if ($z -> is_inf("+")) {
                return $x -> bnan(@r);
            } else {
                return $x -> binf("-", @r);
            }
        } else {                                # -inf < x < 0, -inf < y < +inf
            if ($z -> is_inf("-")) {
                return $x -> binf("-", @r);
            } elsif ($z -> is_inf("+")) {
                return $x -> binf("+", @r);
            }
        }

    } elsif ($x -> is_zero()) {

        if ($y -> is_inf("-")) {                # x = 0, y = -inf
            return $x -> bnan(@r);
        } elsif ($y -> is_inf("+")) {           # x = 0, y = +inf
            return $x -> bnan(@r);
        } else {                                # x = 0, -inf < y < +inf
            if ($z -> is_inf("-")) {
                return $x -> binf("-", @r);
            } elsif ($z -> is_inf("+")) {
                return $x -> binf("+", @r);
            }
        }

    } elsif ($x -> is_pos()) {

        if ($y -> is_inf("-")) {                # 0 < x < +inf, y = -inf
            if ($z -> is_inf("+")) {
                return $x -> bnan(@r);
            } else {
                return $x -> binf("-", @r);
            }
        } elsif ($y -> is_inf("+")) {           # 0 < x < +inf, y = +inf
            if ($z -> is_inf("-")) {
                return $x -> bnan(@r);
            } else {
                return $x -> binf("+", @r);
            }
        } else {                                # 0 < x < +inf, -inf < y < +inf
            if ($z -> is_inf("-")) {
                return $x -> binf("-", @r);
            } elsif ($z -> is_inf("+")) {
                return $x -> binf("+", @r);
            }
        }
    }

    # The code below might be faster if we compute the GCD earlier than in the
    # call to bnorm().
    #
    #   xs * xn   ys * yn   zs * zn      / xs: sign of x        \
    #   ------- * ------- + -------      | xn: numerator of x   |
    #      xd        yd        zd        | xd: denominator of x |
    #                                    \ ditto for y and z    /
    #   xs * ys * xn * yn   zs * zn
    # = ----------------- + -------
    #        xd * yd          zd
    #
    #   xs * ys * xn * yn * zd + zs * xd * yd * zn
    # = ------------------------------------------
    #                  xd * yd * zd

    my $xn_yn    = $LIB -> _mul($LIB -> _copy($x->{_n}), $y->{_n});
    my $xn_yn_zd = $LIB -> _mul($xn_yn, $z->{_d});

    my $xd_yd    = $LIB -> _mul($x->{_d}, $y->{_d});
    my $xd_yd_zn = $LIB -> _mul($LIB -> _copy($xd_yd), $z->{_n});

    my $xd_yd_zd = $LIB -> _mul($xd_yd, $z->{_d});

    my $sgn1 = $x->{sign} eq $y->{sign} ? "+" : "-";
    my $sgn2 = $z->{sign};

    ($x->{_n}, $x->{sign}) = $LIB -> _sadd($xn_yn_zd, $sgn1,
                                           $xd_yd_zn, $sgn2);
    $x->{_d} = $xd_yd_zd;
    $x -> bnorm();

    return $x;
}

sub bmodpow {
    # set up parameters
    my ($class, $x, $y, $m, @r)
      = ref($_[0]) && ref($_[0]) eq ref($_[1]) && ref($_[1]) eq ref($_[2])
      ? (ref($_[0]), @_)
      : objectify(3, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bmodpow');

    # Convert $x, $y, and $m into Math::BigInt objects.

    my $xint = Math::BigInt -> new($x -> copy() -> bint());
    my $yint = Math::BigInt -> new($y -> copy() -> bint());
    my $mint = Math::BigInt -> new($m -> copy() -> bint());

    $xint -> bmodpow($yint, $mint, @r);
    my $xtmp = Math::BigRat -> new($xint -> bfstr());

    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};
    return $x;
}

sub bmodinv {
    # set up parameters
    my ($class, $x, $y, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_)
                            : objectify(2, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bmodinv');

    # Convert $x and $y into Math::BigInt objects.

    my $xint = Math::BigInt -> new($x -> copy() -> bint());
    my $yint = Math::BigInt -> new($y -> copy() -> bint());

    $xint -> bmodinv($yint, @r);
    my $xtmp = Math::BigRat -> new($xint -> bfstr());

    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};
    return $x;
}

sub blog {
    # Return the logarithm of the operand. If a second operand is defined, that
    # value is used as the base, otherwise the base is assumed to be Euler's
    # constant.

    my ($class, $x, $base, @r);

    # Don't objectify the base, since an undefined base, as in $x->blog() or
    # $x->blog(undef) signals that the base is Euler's number.

    if (!ref($_[0]) && $_[0] =~ /^[A-Za-z]|::/) {
        # E.g., Math::BigRat->blog(256, 2)
        ($class, $x, $base, @r) =
          defined $_[2] ? objectify(2, @_) : objectify(1, @_);
    } else {
        # E.g., Math::BigRat::blog(256, 2) or $x->blog(2)
        ($class, $x, $base, @r) =
          defined $_[1] ? objectify(2, @_) : objectify(1, @_);
    }

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('blog');

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

    # Now take care of the cases where $x and/or $base is 1/N.
    #
    #   log(1/N) / log(B)   = -log(N)/log(B)
    #   log(1/N) / log(1/B) =  log(N)/log(B)
    #   log(N)   / log(1/B) = -log(N)/log(B)

    my $neg = 0;
    if ($x -> numerator() -> is_one()) {
        $x -> binv();
        $neg = !$neg;
    }
    if (defined(blessed($base)) && $base -> isa($class)) {
        if ($base -> numerator() -> is_one()) {
            $base = $base -> copy() -> binv();
            $neg = !$neg;
        }
    }

    # disable upgrading and downgrading

    require Math::BigFloat;
    my $upg = Math::BigFloat -> upgrade();
    my $dng = Math::BigFloat -> downgrade();
    Math::BigFloat -> upgrade(undef);
    Math::BigFloat -> downgrade(undef);

    # At this point we are done handling all exception cases and trivial cases.

    $base = Math::BigFloat -> new($base) if defined $base;
    my $xnum = Math::BigFloat -> new($LIB -> _str($x->{_n}));
    my $xden = Math::BigFloat -> new($LIB -> _str($x->{_d}));
    my $xstr = $xnum -> bfdiv($xden) -> blog($base, @r) -> bfstr();

    # reset upgrading and downgrading

    Math::BigFloat -> upgrade($upg);
    Math::BigFloat -> downgrade($dng);

    my $xobj = Math::BigRat -> new($xstr);
    $x -> {sign} = $xobj -> {sign};
    $x -> {_n}   = $xobj -> {_n};
    $x -> {_d}   = $xobj -> {_d};

    return $neg ? $x -> bneg() : $x;
}

sub bexp {
    # set up parameters
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bexp');

    return $x -> binf(@r)  if $x -> is_inf("+");
    return $x -> bzero(@r) if $x -> is_inf("-");

    # we need to limit the accuracy to protect against overflow
    my $fallback = 0;
    my ($scale, @params);
    ($x, @params) = $x->_find_round_parameters(@r);

    # also takes care of the "error in _find_round_parameters?" case
    return $x if $x -> is_nan();

    # no rounding at all, so must use fallback
    if (scalar @params == 0) {
        # simulate old behaviour
        $params[0] = $class -> div_scale(); # and round to it as accuracy
        $params[1] = undef;              # P = undef
        $scale = $params[0]+4;           # at least four more for proper round
        $params[2] = $r[2];              # round mode by caller or undef
        $fallback = 1;                   # to clear a/p afterwards
    } else {
        # the 4 below is empirical, and there might be cases where it's not enough...
        $scale = abs($params[0] || $params[1]) + 4; # take whatever is defined
    }

    return $x -> bone(@params) if $x -> is_zero();

    # See the comments in Math::BigFloat on how this algorithm works.
    # Basically we calculate A and B (where B is faculty(N)) so that A/B = e

    my $x_org = $x -> copy();
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
    if (!$x_org -> is_one()) {
        # raise $x to the wanted power and round it in one step:
        $x -> bpow($x_org, @params);
    } else {
        # else just round the already computed result
        delete $x->{accuracy}; delete $x->{precision};
        # shortcut to not run through _find_round_parameters again
        if (defined $params[0]) {
            $x -> bround($params[0], $params[2]); # then round accordingly
        } else {
            $x -> bfround($params[1], $params[2]); # then round accordingly
        }
    }
    if ($fallback) {
        # clear a/p after round, since user did not request it
        delete $x->{accuracy}; delete $x->{precision};
    }

    $x;
}

sub bilog2 {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bilog2');

    return $x -> bnan(@r)        if $x -> is_nan();
    return $x -> binf("+", @r)   if $x -> is_inf("+");
    return $x -> binf("-", @r)   if $x -> is_zero();

    if ($x -> is_neg()) {
        return $x -> _upg() -> bilog2(@r) if $class -> upgrade();
        return $x -> bnan(@r);
    }

    $x->{_n} = $LIB -> _div($x->{_n}, $x->{_d});
    $x->{_n} = $LIB -> _ilog2($x->{_n});
    $x->{_d} = $LIB -> _one();
    $x -> bnorm() -> round(@r);
    $x -> _dng();
    return $x;
}

sub bilog10 {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bilog10');

    return $x -> bnan(@r)        if $x -> is_nan();
    return $x -> binf("+", @r)   if $x -> is_inf("+");
    return $x -> binf("-", @r)   if $x -> is_zero();

    if ($x -> is_neg()) {
        return $x -> _upg() -> bilog10(@r) if $class -> upgrade();
        return $x -> bnan(@r);
    }

    $x->{_n} = $LIB -> _div($x->{_n}, $x->{_d});
    $x->{_n} = $LIB -> _ilog10($x->{_n});
    $x->{_d} = $LIB -> _one();
    $x -> bnorm() -> round(@r);
    $x -> _dng();
    return $x;
}

sub bclog2 {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bclog2');

    return $x -> bnan(@r)        if $x -> is_nan();
    return $x -> binf("+", @r)   if $x -> is_inf("+");
    return $x -> binf("-", @r)   if $x -> is_zero();

    if ($x -> is_neg()) {
        return $x -> _upg() -> bclog2(@r) if $class -> upgrade();
        return $x -> bnan(@r);
    }

    $x->{_n} = $LIB -> _div($x->{_n}, $x->{_d});
    $x->{_n} = $LIB -> _clog2($x->{_n});
    $x->{_d} = $LIB -> _one();
    $x -> bnorm() -> round(@r);
    $x -> _dng();
    return $x;
}

sub bclog10 {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bclog10');

    return $x -> bnan(@r)        if $x -> is_nan();
    return $x -> binf("+", @r)   if $x -> is_inf("+");
    return $x -> binf("-", @r)   if $x -> is_zero();

    if ($x -> is_neg()) {
        return $x -> _upg() -> bclog10(@r) if $class -> upgrade();
        return $x -> bnan(@r);
    }

    $x->{_n} = $LIB -> _div($x->{_n}, $x->{_d});
    $x->{_n} = $LIB -> _clog10($x->{_n});
    $x->{_d} = $LIB -> _one();
    $x -> bnorm() -> round(@r);
    $x -> _dng();
    return $x;
}

sub bnok {
    # set up parameters
    my ($class, $x, $y, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_)
                            : objectify(2, @_);

    carp "Rounding is not supported for ", (caller(0))[3], "()" if @r;

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bnok');

    return $x -> bnan() if $x -> is_nan() || $y -> is_nan();
    return $x -> bnan() if (($x -> is_finite() && !$x -> is_int()) ||
                            ($y -> is_finite() && !$y -> is_int()));

    my $xint = Math::BigInt -> new($x -> bstr());
    my $yint = Math::BigInt -> new($y -> bstr());
    $xint -> bnok($yint);
    my $xrat = Math::BigRat -> new($xint);

    $x -> {sign} = $xrat -> {sign};
    $x -> {_n}   = $xrat -> {_n};
    $x -> {_d}   = $xrat -> {_d};

    return $x;
}

sub bperm {
    # set up parameters
    my ($class, $x, $y, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_)
                            : objectify(2, @_);

    carp "Rounding is not supported for ", (caller(0))[3], "()" if @r;

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bperm');

    return $x -> bnan() if $x -> is_nan() || $y -> is_nan();
    return $x -> bnan() if (($x -> is_finite() && !$x -> is_int()) ||
                            ($y -> is_finite() && !$y -> is_int()));

    my $xint = Math::BigInt -> new($x -> bstr());
    my $yint = Math::BigInt -> new($y -> bstr());
    $xint -> bperm($yint);
    my $xrat = Math::BigRat -> new($xint);

    $x -> {sign} = $xrat -> {sign};
    $x -> {_n}   = $xrat -> {_n};
    $x -> {_d}   = $xrat -> {_d};

    return $x;
}

sub bfac {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bfac');

    return $x -> bnan(@r)      if $x -> is_nan() || $x -> is_inf("-");
    return $x -> binf("+", @r) if $x -> is_inf("+");
    return $x -> bnan(@r)      if $x -> is_neg() || !$x -> is_int();
    return $x -> bone(@r)      if $x -> is_zero() || $x -> is_one();

    $x->{_n} = $LIB->_fac($x->{_n});
    # since _d is 1, we don't need to reduce/norm the result
    $x -> round(@r);
    $x -> _dng();
    return $x;
}

sub bdfac {
    # compute double factorial, modify $x in place
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bdfac');

    return $x -> bnan(@r)      if $x -> is_nan() || $x -> is_inf("-");
    return $x -> binf("+", @r) if $x -> is_inf("+");
    return $x -> bnan(@r)      if $x <= -2 || !$x -> is_int();
    return $x -> bone(@r)      if $x <= 1;

    croak("bdfac() requires a newer version of the $LIB library.")
        unless $LIB -> can('_dfac');

    $x->{_n} = $LIB->_dfac($x->{_n});
    # since _d is 1, we don't need to reduce/norm the result
    $x -> round(@r);
    $x -> _dng();
    return $x;
}

sub btfac {
    # compute triple factorial, modify $x in place
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('btfac');

    return $x -> bnan(@r)      if $x -> is_nan() || !$x -> is_int();
    return $x -> binf("+", @r) if $x -> is_inf("+");

    my $k = $class -> new("3");
    return $x -> bnan(@r) if $x <= -$k;

    my $one = $class -> bone();
    return $x -> bone(@r) if $x <= $one;

    my $f = $x -> copy();
    while ($f -> bsub($k) > $one) {
        $x -> bmul($f);
    }
    $x -> round(@r);
    $x -> _dng();
    return $x;
}

sub bmfac {
    # compute multi-factorial

    my ($class, $x, $k, @r) = ref($_[0]) && ref($_[0]) eq ref($_[1])
                            ? (ref($_[0]), @_) : objectify(2, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bmfac');

    return $x -> bnan(@r)      if $x -> is_nan() || $x -> is_inf("-") ||
                                  !$k -> is_pos();
    return $x -> binf("+", @r) if $x -> is_inf("+");
    return $x -> bround(@r)    if $k -> is_inf("+");
    return $x -> bnan(@r)      if !$x -> is_int() || !$k -> is_int();
    return $x -> bnan(@r)      if $k < 1 || $x <= -$k;

    my $one = $class -> bone();
    return $x -> bone(@r) if $x <= $one;

    my $f = $x -> copy();
    while ($f -> bsub($k) > $one) {
        $x -> bmul($f);
    }
    $x -> round(@r);
    $x -> _dng();
    return $x;
}

sub bfib {
    # compute Fibonacci number(s)
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    croak("bfib() requires a newer version of the $LIB library.")
        unless $LIB -> can('_fib');

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bfib');

    # List context.

    if (wantarray) {
        croak("bfib() can't return an infinitely long list of numbers")
          if $x -> is_inf();

        return if $x -> is_nan() || !$x -> is_int();

        # The following places a limit on how large $x can be. Should this
        # limit be removed? XXX

        my $n = $x -> numify();

        my @y;
        {
            $y[0] = $x -> copy() -> babs();
            $y[0]{_n} = $LIB -> _zero();
            $y[0]{_d} = $LIB -> _one();
            last if $n == 0;

            $y[1] = $y[0] -> copy();
            $y[1]{_n} = $LIB -> _one();
            $y[1]{_d} = $LIB -> _one();
            last if $n == 1;

            for (my $i = 2 ; $i <= abs($n) ; $i++) {
                $y[$i] = $y[$i - 1] -> copy();
                $y[$i]{_n} = $LIB -> _add($LIB -> _copy($y[$i - 1]{_n}),
                                                        $y[$i - 2]{_n});
            }

            # If negative, insert sign as appropriate.

            if ($x -> is_neg()) {
                for (my $i = 2 ; $i <= $#y ; $i += 2) {
                    $y[$i]{sign} = '-';
                }
            }

            # The last element in the array is the invocand.

            $x->{sign} = $y[-1]{sign};
            $x->{_n}   = $y[-1]{_n};
            $x->{_d}   = $y[-1]{_d};
            $y[-1] = $x;
        }

        for (@y) {
            $_ -> bnorm();
            $_ -> round(@r);
        }

        return @y;
    }

    # Scalar context.

    else {
        return $x if $x -> is_inf('+');
        return $x -> bnan() if $x -> is_nan() || $x -> is_inf('-') ||
                              !$x -> is_int();

        $x->{sign}  = $x -> is_neg() && $x -> is_even() ? '-' : '+';
        $x->{_n} = $LIB -> _fib($x->{_n});
        $x->{_d} = $LIB -> _one();
        $x -> bnorm();
        return $x -> round(@r);
    }
}

sub blucas {
    # compute Lucas number(s)
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    croak("blucas() requires a newer version of the $LIB library.")
        unless $LIB -> can('_lucas');

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('blucas');

    # List context.

    if (wantarray) {
        croak("blucas() can't return an infinitely long list of numbers")
          if $x -> is_inf();

        return if $x -> is_nan() || !$x -> is_int();

        # The following places a limit on how large $x can be, at least on 32
        # bit systems. Should this limit be removed? XXX

        my $n = $x -> numify();

        my @y;
        {
            $y[0] = $x -> copy() -> babs();
            $y[0]{_n} = $LIB -> _two();
            last if $n == 0;

            $y[1] = $y[0] -> copy();
            $y[1]{_n} = $LIB -> _one();
            last if $n == 1;

            for (my $i = 2 ; $i <= abs($n) ; $i++) {
                $y[$i] = $y[$i - 1] -> copy();
                $y[$i]{_n} = $LIB -> _add($LIB -> _copy($y[$i - 1]{_n}),
                                          $y[$i - 2]{_n});
            }

            # If negative, insert sign as appropriate.

            if ($x -> is_neg()) {
                for (my $i = 2 ; $i <= $#y ; $i += 2) {
                    $y[$i]{sign} = '-';
                }
            }

            # The last element in the array is the invocand.

            $x->{_n}   = $y[-1]{_n};
            $x->{sign} = $y[-1]{sign};
            $y[-1] = $x;
        }

        @y = map { $_ -> round(@r) } @y;
        return @y;
    }

    # Scalar context.

    else {
        return $x if $x -> is_inf('+');
        return $x -> bnan() if $x -> is_nan() || $x -> is_inf('-') ||
                              !$x -> is_int();

        $x->{sign}  = $x -> is_neg() && $x -> is_even() ? '-' : '+';
        $x->{_n} = $LIB -> _lucas($x->{_n});
        return $x -> round(@r);
    }
}

sub blsft {
    my ($class, $x, $y, $b, @r);

    # Objectify the base only when it is defined, since an undefined base, as
    # in $x->blsft(3) or $x->blog(3, undef) means use the default base 2.

    if (!ref($_[0]) && $_[0] =~ /^[A-Za-z]|::/) {
        # E.g., Math::BigInt->blog(256, 5, 2)
        ($class, $x, $y, $b, @r) =
          defined $_[3] ? objectify(3, @_) : objectify(2, @_);
    } else {
        # E.g., Math::BigInt::blog(256, 5, 2) or $x->blog(5, 2)
        ($class, $x, $y, $b, @r) =
          defined $_[2] ? objectify(3, @_) : objectify(2, @_);
    }

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('blsft');

    $b = 2 unless defined($b);
    $b = $class -> new($b) unless ref($b) && $b -> isa($class);

    return $x -> bnan() if $x -> is_nan() || $y -> is_nan() || $b -> is_nan();

    # shift by a negative amount?
    return $x -> brsft($y -> copy() -> babs(), $b) if $y -> {sign} =~ /^-/;

    $x -> bmul($b -> bpow($y));
}

sub brsft {
    my ($class, $x, $y, $b, @r);

    # Objectify the base only when it is defined, since an undefined base, as
    # in $x->blsft(3) or $x->blog(3, undef) means use the default base 2.

    if (!ref($_[0]) && $_[0] =~ /^[A-Za-z]|::/) {
        # E.g., Math::BigInt->blog(256, 5, 2)
        ($class, $x, $y, $b, @r) =
          defined $_[3] ? objectify(3, @_) : objectify(2, @_);
    } else {
        # E.g., Math::BigInt::blog(256, 5, 2) or $x->blog(5, 2)
        ($class, $x, $y, $b, @r) =
          defined $_[2] ? objectify(3, @_) : objectify(2, @_);
    }

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('brsft');

    $b = 2 unless defined($b);
    $b = $class -> new($b) unless ref($b) && $b -> isa($class);

    return $x -> bnan() if $x -> is_nan() || $y -> is_nan() || $b -> is_nan();

    # shift by a negative amount?
    return $x -> blsft($y -> copy() -> babs(), $b) if $y -> {sign} =~ /^-/;

    # the following call to bfdiv() will return either quotient (scalar context)
    # or quotient and remainder (list context).
    $x -> bfdiv($b -> bpow($y));
}

###############################################################################
# Bitwise methods
###############################################################################

# Bitwise left shift.

sub bblsft {
    # We don't call objectify(), because the bitwise methods should not
    # upgrade, even when upgrading is enabled.

    my ($class, $x, $y, @r) = ref($_[0]) ? (ref($_[0]), @_) : @_;

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bblsft');

    # Let Math::BigInt do the job.

    my $xint = Math::BigInt -> bblsft($x, $y, @r);

    # Temporarily disable downgrading.

    my $dng = $class -> downgrade();
    $class -> downgrade(undef);

    # Convert to our class without downgrading.

    my $xrat = $class -> new($xint);

    # Reset downgrading.

    $class -> downgrade($dng);

    # If we are called as a class method, the first operand might not be an
    # object of this class, so check.

    if (defined(blessed($x)) && $x -> isa(__PACKAGE__)) {
        $x -> {sign} = $xrat -> {sign};
        $x -> {_n}   = $xrat -> {_n};
        $x -> {_d}   = $xrat -> {_d};
    } else {
        $x = $xrat;
    }

    # Now we might downgrade.

    $x -> round(@r);
    $x -> _dng();
    return $x;
}

# Bitwise right shift.

sub bbrsft {
    # We don't call objectify(), because the bitwise methods should not
    # upgrade/downgrade, even when upgrading/downgrading is enabled.

    my ($class, $x, $y, @r) = ref($_[0]) ? (ref($_[0]), @_) : @_;

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bbrsft');

    # Let Math::BigInt do the job.

    my $xint = Math::BigInt -> bbrsft($x, $y, @r);

    # Temporarily disable downgrading.

    my $dng = $class -> downgrade();
    $class -> downgrade(undef);

    # Convert to our class without downgrading.

    my $xrat = $class -> new($xint);

    # Reset downgrading.

    $class -> downgrade($dng);

    # If we are called as a class method, the first operand might not be an
    # object of this class, so check.

    if (defined(blessed($x)) && $x -> isa(__PACKAGE__)) {
        $x -> {sign} = $xrat -> {sign};
        $x -> {_n}   = $xrat -> {_n};
        $x -> {_d}   = $xrat -> {_d};
    } else {
        $x = $xrat;
    }

    # Now we might downgrade.

    $x -> round(@r);
    $x -> _dng();
    return $x;
}

sub band {
    my $x     = shift;
    my $xref  = ref($x);
    my $class = $xref || $x;

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('band');

    croak 'band() is an instance method, not a class method' unless $xref;
    croak 'Not enough arguments for band()' if @_ < 1;

    my $y = shift;
    $y = $class -> new($y) unless ref($y);

    my @r = @_;

    my $xtmp = $x -> as_int() -> band($y -> as_int()) -> as_rat();
    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};

    return $x -> round(@r);
}

sub bior {
    my $x     = shift;
    my $xref  = ref($x);
    my $class = $xref || $x;

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bior');

    croak 'bior() is an instance method, not a class method' unless $xref;
    croak 'Not enough arguments for bior()' if @_ < 1;

    my $y = shift;
    $y = $class -> new($y) unless ref($y);

    my @r = @_;

    my $xtmp = $x -> as_int() -> bior($y -> as_int()) -> as_rat();
    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};

    return $x -> round(@r);
}

sub bxor {
    my $x     = shift;
    my $xref  = ref($x);
    my $class = $xref || $x;

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bxor');

    croak 'bxor() is an instance method, not a class method' unless $xref;
    croak 'Not enough arguments for bxor()' if @_ < 1;

    my $y = shift;
    $y = $class -> new($y) unless ref($y);

    my @r = @_;

    my $xtmp = $x -> as_int() -> bxor($y -> as_int()) -> as_rat();
    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};

    return $x -> round(@r);
}

sub bnot {
    my $x     = shift;
    my $xref  = ref($x);
    my $class = $xref || $x;

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bnot');

    croak 'bnot() is an instance method, not a class method' unless $xref;

    my @r = @_;

    my $xtmp = $x -> as_int() -> bnot() -> as_rat();
    $x -> {sign} = $xtmp -> {sign};
    $x -> {_n}   = $xtmp -> {_n};
    $x -> {_d}   = $xtmp -> {_d};

    return $x -> round(@r);
}

##############################################################################
# round

sub round {
    my $x = shift;

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('round');

    $x -> _dng() if ($x -> is_int() ||
                     $x -> is_inf() ||
                     $x -> is_nan());
    $x;
}

sub bround {
    my $x = shift;

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bround');

    $x -> _dng() if ($x -> is_int() ||
                     $x -> is_inf() ||
                     $x -> is_nan());
    $x;
}

sub bfround {
    my $x = shift;

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bfround');

    $x -> _dng() if ($x -> is_int() ||
                     $x -> is_inf() ||
                     $x -> is_nan());
    $x;
}

sub bfloor {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bfloor');

    return $x -> bnan(@r) if $x -> is_nan();

    if (!$x -> is_finite() ||           # NaN or inf or
        $LIB->_is_one($x->{_d}))        # integer
    {
        $x -> round(@r);
        $x -> _dng();
        return $x;
    }

    $x->{_n} = $LIB->_div($x->{_n}, $x->{_d});  # 22/7 => 3/1 w/ truncate
    $x->{_d} = $LIB->_one();                    # d => 1
    $x->{_n} = $LIB->_inc($x->{_n}) if $x->{sign} eq '-';   # -22/7 => -4/1

    $x -> round(@r);
    $x -> _dng();
    return $x;
}

sub bceil {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bceil');

    return $x -> bnan(@r) if $x -> is_nan();

    if (!$x -> is_finite() ||           # NaN or inf or
        $LIB->_is_one($x->{_d}))        # integer
    {
        $x -> round(@r);
        $x -> _dng();
        return $x;
    }

    $x->{_n} = $LIB->_div($x->{_n}, $x->{_d});  # 22/7 => 3/1 w/ truncate
    $x->{_d} = $LIB->_one();                    # d => 1
    $x->{_n} = $LIB->_inc($x->{_n}) if $x->{sign} eq '+';   # +22/7 => 4/1
    $x->{sign} = '+' if $x->{sign} eq '-' && $LIB->_is_zero($x->{_n}); # -0 => 0

    $x -> round(@r);
    $x -> _dng();
    return $x;
}

sub bint {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Don't modify constant (read-only) objects.

    return $x if $x -> modify('bint');

    return $x -> bnan(@r) if $x -> is_nan();

    if (!$x -> is_finite() ||           # NaN or inf or
        $LIB->_is_one($x->{_d}))        # integer
    {
        $x -> round(@r);
        $x -> _dng();
        return $x;
    }

    $x->{_n} = $LIB->_div($x->{_n}, $x->{_d});  # 22/7 => 3/1 w/ truncate
    $x->{_d} = $LIB->_one();                    # d => 1
    $x->{sign} = '+' if $x->{sign} eq '-' && $LIB -> _is_zero($x->{_n});

    $x -> round(@r);
    $x -> _dng();
    return $x;
}

sub bgcd {
    # GCD -- Euclid's algorithm, variant C (Knuth Vol 3, pg 341 ff)

    # Class::method(...) -> Class->method(...)
    unless (@_ && (defined(blessed($_[0])) && $_[0] -> isa(__PACKAGE__) ||
                   ($_[0] =~ /^[a-z]\w*(?:::[a-z]\w*)*$/i &&
                    $_[0] !~ /^(inf|nan)/i)))
    {
        #carp "Using ", (caller(0))[3], "() as a function is deprecated;",
        #  " use is as a method instead";
        unshift @_, __PACKAGE__;
    }

    my ($class, @args) = objectify(0, @_);

    # Pre-process list of operands.

    for my $arg (@args) {
        return $class -> bnan() unless $arg -> is_finite();
    }

    # Temporarily disable downgrading.

    my $dng = $class -> downgrade();
    $class -> downgrade(undef);

    my $x = shift @args;
    $x = $x -> copy();          # bgcd() and blcm() never modify any operands

    while (@args) {
        my $y = shift @args;

        # greatest common divisor
        while (! $y -> is_zero()) {
            ($x, $y) = ($y -> copy(), $x -> copy() -> bmod($y));
        }

        last if $x -> is_one();
    }
    $x -> babs();

    # Restore downgrading.

    $class -> downgrade($dng);

    $x -> _dng() if $x -> is_int();
    return $x;
}

sub blcm {
    # Least Common Multiple

    # Class::method(...) -> Class->method(...)
    unless (@_ && (defined(blessed($_[0])) && $_[0] -> isa(__PACKAGE__) ||
                   ($_[0] =~ /^[a-z]\w*(?:::[a-z]\w*)*$/i &&
                    $_[0] !~ /^(inf|nan)/i)))
    {
        #carp "Using ", (caller(0))[3], "() as a function is deprecated;",
        #  " use is as a method instead";
        unshift @_, __PACKAGE__;
    }

    my ($class, @args) = objectify(0, @_);

    # Pre-process list of operands.

    for my $arg (@args) {
        return $class -> bnan() unless $arg -> is_finite();
    }

    for my $arg (@args) {
        return $class -> bzero() if $arg -> is_zero();
    }

    my $x = shift @args;
    $x = $x -> copy();          # bgcd() and blcm() never modify any operands

    while (@args) {
        my $y = shift @args;
        my $gcd = $x -> copy() -> bgcd($y);
        $x -> bdiv($gcd) -> bmul($y);
    }

    $x -> babs();       # might downgrade
    return $x;
}

sub digit {
    my ($class, $x, $n) = ref($_[0]) ? (undef, $_[0], $_[1]) : objectify(1, @_);

    return $nan unless $x -> is_int();
    $LIB->_digit($x->{_n}, $n || 0); # digit(-123/1, 2) => digit(123, 2)
}

sub length {
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return $nan unless $x -> is_int();
    $LIB->_len($x->{_n});       # length(-123/1) => length(123)
}

sub parts {
    my ($class, $x) = ref($_[0]) ? (ref($_[0]), $_[0]) : objectify(1, @_);

    my $c = 'Math::BigInt';

    return ($c -> bnan(), $c -> bnan()) if $x -> is_nan();
    return ($c -> binf(), $c -> binf()) if $x -> is_inf("+");
    return ($c -> binf('-'), $c -> binf()) if $x -> is_inf("-");

    my $n = $c -> new($LIB->_str($x->{_n}));
    $n->{sign} = $x->{sign};
    my $d = $c -> new($LIB->_str($x->{_d}));
    ($n, $d);
}

sub dparts {
    my $x = shift;
    my $class = ref $x;

    croak("dparts() is an instance method") unless $class;

    if ($x -> is_nan()) {
        return $class -> bnan(), $class -> bnan() if wantarray;
        return $class -> bnan();
    }

    if ($x -> is_inf()) {
        return $class -> binf($x -> sign()), $class -> bzero() if wantarray;
        return $class -> binf($x -> sign());
    }

    # 355/113 => 3 + 16/113

    my ($q, $r)  = $LIB -> _div($LIB -> _copy($x -> {_n}), $x -> {_d});

    my $int = Math::BigRat -> new($x -> {sign} . $LIB -> _str($q));
    return $int unless wantarray;

    my $frc = Math::BigRat -> new($x -> {sign} . $LIB -> _str($r),
                                  $LIB -> _str($x -> {_d}));

    return $int, $frc;
}

sub fparts {
    my $x = shift;
    my $class = ref $x;

    croak("fparts() is an instance method") unless $class;

    return ($class -> bnan(),
            $class -> bnan()) if $x -> is_nan();

    my $numer = $x -> copy();
    my $denom = $class -> bzero();

    $denom -> {_n} = $numer -> {_d};
    $numer -> {_d} = $LIB -> _one();

    return $numer, $denom;
}

sub numerator {
    my ($class, $x) = ref($_[0]) ? (ref($_[0]), $_[0]) : objectify(1, @_);

    # NaN, inf, -inf
    return Math::BigInt -> new($x->{sign}) if !$x -> is_finite();

    my $n = Math::BigInt -> new($LIB->_str($x->{_n}));
    $n->{sign} = $x->{sign};
    $n;
}

sub denominator {
    my ($class, $x) = ref($_[0]) ? (ref($_[0]), $_[0]) : objectify(1, @_);

    # NaN
    return Math::BigInt -> new($x->{sign}) if $x -> is_nan();
    # inf, -inf
    return Math::BigInt -> bone() if !$x -> is_finite();

    Math::BigInt -> new($LIB->_str($x->{_d}));
}

###############################################################################
# String conversion methods
###############################################################################

sub bstr {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), $_[0]) : objectify(1, @_);

    carp "Rounding is not supported for ", (caller(0))[3], "()" if @r;

    # Inf and NaN

    if (!$x -> is_finite()) {
        return $x->{sign} unless $x -> is_inf("+");     # -inf, NaN
        return 'inf';                                   # +inf
    }

    # Upgrade?

    return $x -> _upg() -> bstr(@r)
      if $class -> upgrade() && !$x -> isa($class);

    # Finite number

    my $s = '';
    $s = $x->{sign} if $x->{sign} ne '+';       # '+3/2' => '3/2'

    my $str = $x->{sign} eq '-' ? '-' : '';
    $str .= $LIB->_str($x->{_n});
    $str .= '/' . $LIB->_str($x->{_d}) unless $LIB -> _is_one($x->{_d});
    return $str;
}

sub bsstr {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), $_[0]) : objectify(1, @_);

    carp "Rounding is not supported for ", (caller(0))[3], "()" if @r;

    # Inf and NaN

    if (!$x -> is_finite()) {
        return $x->{sign} unless $x -> is_inf("+");     # -inf, NaN
        return 'inf';                                   # +inf
    }

    # Upgrade?

    return $x -> _upg() -> bsstr(@r)
      if $class -> upgrade() && !$x -> isa($class);

    # Finite number

    my $str = $x->{sign} eq '-' ? '-' : '';
    $str .= $LIB->_str($x->{_n});
    $str .= '/' . $LIB->_str($x->{_d}) unless $LIB -> _is_one($x->{_d});
    return $str;
}

sub bnstr {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Inf and NaN

    if ($x->{sign} ne '+' && $x->{sign} ne '-') {
        return $x->{sign} unless $x -> is_inf("+");     # -inf, NaN
        return 'inf';                                   # +inf
    }

    # Upgrade?

    $x -> _upg() -> bnstr(@r)
      if $class -> upgrade() && !$x -> isa(__PACKAGE__);

    return $x -> as_float(@r) -> bnstr();
}

sub bestr {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Inf and NaN

    if ($x->{sign} ne '+' && $x->{sign} ne '-') {
        return $x->{sign} unless $x -> is_inf("+");     # -inf, NaN
        return 'inf';                                   # +inf
    }

    # Upgrade?

    $x -> _upg() -> bestr(@r)
      if $class -> upgrade() && !$x -> isa(__PACKAGE__);

    return $x -> as_float(@r) -> bestr();
}

sub bdstr {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    # Inf and NaN

    if ($x->{sign} ne '+' && $x->{sign} ne '-') {
        return $x->{sign} unless $x -> is_inf("+");     # -inf, NaN
        return 'inf';                                   # +inf
    }

    return ($x->{sign} eq '-' ? '-' : '') . $LIB->_str($x->{_n})
      if $x -> is_int();

    # Upgrade?

    $x -> _upg() -> bdstr(@r)
      if $class -> upgrade() && !$x -> isa(__PACKAGE__);

    # Integer number

    return $x -> as_float(@r) -> bdstr();
}

sub bfstr {
    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), $_[0]) : objectify(1, @_);

    carp "Rounding is not supported for ", (caller(0))[3], "()" if @r;

    # Inf and NaN

    if (!$x -> is_finite()) {
        return $x->{sign} unless $x -> is_inf("+");     # -inf, NaN
        return 'inf';                                   # +inf
    }

    # Upgrade?

    return $x -> _upg() -> bfstr(@r)
      if $class -> upgrade() && !$x -> isa($class);

    # Finite number

    my $str = $x->{sign} eq '-' ? '-' : '';
    $str .= $LIB->_str($x->{_n});
    $str .= '/' . $LIB->_str($x->{_d}) unless $LIB -> _is_one($x->{_d});
    return $str;
}

sub to_hex {
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    # Inf and NaN

    if (!$x -> is_finite()) {
        return $x->{sign} unless $x -> is_inf("+");     # -inf, NaN
        return 'inf';                                   # +inf
    }

    return $nan unless $x -> is_int();

    my $str = $LIB->_to_hex($x->{_n});
    return $x->{sign} eq "-" ? "-$str" : $str;
}

sub to_oct {
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    # Inf and NaN

    if (!$x -> is_finite()) {
        return $x->{sign} unless $x -> is_inf("+");     # -inf, NaN
        return 'inf';                                   # +inf
    }

    return $nan unless $x -> is_int();

    my $str = $LIB->_to_oct($x->{_n});
    return $x->{sign} eq "-" ? "-$str" : $str;
}

sub to_bin {
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    # Inf and NaN

    if (!$x -> is_finite()) {
        return $x->{sign} unless $x -> is_inf("+");     # -inf, NaN
        return 'inf';                                   # +inf
    }

    return $nan unless $x -> is_int();

    my $str = $LIB->_to_bin($x->{_n});
    return $x->{sign} eq "-" ? "-$str" : $str;
}

sub to_bytes {
    # return a byte string

    my ($class, $x, @r) = ref($_[0]) ? (ref($_[0]), @_) : objectify(1, @_);

    carp "Rounding is not supported for ", (caller(0))[3], "()" if @r;

    croak("to_bytes() requires a finite, non-negative integer")
        if $x -> is_neg() || ! $x -> is_int();

    return $x -> _upg() -> to_bytes(@r)
      if $class -> upgrade() && !$x -> isa(__PACKAGE__);

    croak("to_bytes() requires a newer version of the $LIB library.")
        unless $LIB -> can('_to_bytes');

    return $LIB->_to_bytes($x->{_n});
}

sub to_ieee754 {
    my ($class, $x, $format, @r) = ref($_[0]) ? (ref($_[0]), @_)
                                              : objectify(1, @_);

    carp "Rounding is not supported for ", (caller(0))[3], "()" if @r;

    return $x -> as_float() -> to_ieee754($format);
}

sub as_hex {
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return $x unless $x -> is_int();

    my $s = $x->{sign}; $s = '' if $s eq '+';
    $s . $LIB->_as_hex($x->{_n});
}

sub as_oct {
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return $x unless $x -> is_int();

    my $s = $x->{sign}; $s = '' if $s eq '+';
    $s . $LIB->_as_oct($x->{_n});
}

sub as_bin {
    my ($class, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    return $x unless $x -> is_int();

    my $s = $x->{sign};
    $s = '' if $s eq '+';
    $s . $LIB->_as_bin($x->{_n});
}

sub numify {
    # convert 17/8 => float (aka 2.125)
    my ($self, $x) = ref($_[0]) ? (undef, $_[0]) : objectify(1, @_);

    # Non-finite number.

    if ($x -> is_nan()) {
        require Math::Complex;
        my $inf = $Math::Complex::Inf;
        return $inf - $inf;
    }

    if ($x -> is_inf()) {
        require Math::Complex;
        my $inf = $Math::Complex::Inf;
        return $x -> is_negative() ? -$inf : $inf;
    }

    # Finite number.

    my $abs = $LIB->_is_one($x->{_d})
            ? $LIB->_num($x->{_n})
            : Math::BigFloat -> new($LIB->_str($x->{_n}))
                             -> bfdiv($LIB->_str($x->{_d}))
                             -> bstr();
    return $x->{sign} eq '-' ? 0 - $abs : 0 + $abs;
}

##############################################################################
# import

sub import {
    my $class = shift;
    $IMPORT++;                  # remember we did import()
    my @a;                      # unrecognized arguments

    my @import = ();

    while (@_) {
        my $param = shift;

        # Enable overloading of constants.

        if ($param eq ':constant') {
            overload::constant

                integer => sub {
                    $class -> new(shift);
                },

                float   => sub {
                    $class -> new(shift);
                },

                binary  => sub {
                    # E.g., a literal 0377 shall result in an object whose value
                    # is decimal 255, but new("0377") returns decimal 377.
                    return $class -> from_oct($_[0]) if $_[0] =~ /^0_*[0-7]/;
                    $class -> new(shift);
                };
            next;
        }

        # Upgrading.

        if ($param eq 'upgrade') {
            $class -> upgrade(shift);
            next;
        }

        # Downgrading.

        if ($param eq 'downgrade') {
            $class -> downgrade(shift);
            next;
        }

        # Accuracy.

        if ($param eq 'accuracy') {
            $class -> accuracy(shift);
            next;
        }

        # Precision.

        if ($param eq 'precision') {
            $class -> precision(shift);
            next;
        }

        # Rounding mode.

        if ($param eq 'round_mode') {
            $class -> round_mode(shift);
            next;
        }

        # Fall-back accuracy.

        if ($param eq 'div_scale') {
            $class -> div_scale(shift);
            next;
        }

        # Backend library.

        if ($param =~ /^(lib|try|only)\z/) {
            push @import, $param;
            push @import, shift() if @_;
            next;
        }

        if ($param eq 'with') {
            # alternative class for our private parts()
            # XXX: no longer supported
            # $LIB = shift() || 'Calc';
            # carp "'with' is no longer supported, use 'lib', 'try', or 'only'";
            shift;
            next;
        }

        # Unrecognized parameter.

        push @a, $param;
    }

    Math::BigInt -> import(@import);

    # find out which library was actually loaded
    $LIB = Math::BigInt -> config("lib");

    $class -> SUPER::import(@a);                        # for subclasses
    $class -> export_to_level(1, $class, @a) if @a;     # need this, too
}

1;

__END__

=pod

=head1 NAME

Math::BigRat - arbitrary size rational number math package

=head1 SYNOPSIS

  use Math::BigRat;

  # Generic constructor method (always returns a new object)

  $x = Math::BigRat->new($str);             # defaults to 0
  $x = Math::BigRat->new('256');            # from decimal
  $x = Math::BigRat->new('0256');           # from decimal
  $x = Math::BigRat->new('0xcafe');         # from hexadecimal
  $x = Math::BigRat->new('0x1.fap+7');      # from hexadecimal
  $x = Math::BigRat->new('0o377');          # from octal
  $x = Math::BigRat->new('0o1.35p+6');      # from octal
  $x = Math::BigRat->new('0b101');          # from binary
  $x = Math::BigRat->new('0b1.101p+3');     # from binary

  # Specific constructor methods (no prefix needed; when used as
  # instance method, the value is assigned to the invocand)

  $x = Math::BigRat->from_dec('234');       # from decimal
  $x = Math::BigRat->from_hex('cafe');      # from hexadecimal
  $x = Math::BigRat->from_hex('1.fap+7');   # from hexadecimal
  $x = Math::BigRat->from_oct('377');       # from octal
  $x = Math::BigRat->from_oct('1.35p+6');   # from octal
  $x = Math::BigRat->from_bin('1101');      # from binary
  $x = Math::BigRat->from_bin('1.101p+3');  # from binary
  $x = Math::BigRat->from_bytes($bytes);    # from byte string
  $x = Math::BigRat->from_base('why', 36);  # from any base
  $x = Math::BigRat->from_base_num([1, 0], 2);  # from any base
  $x = Math::BigRat->from_ieee754($b, $fmt);    # from IEEE-754 bytes
  $x = Math::BigRat->bzero();               # create a +0
  $x = Math::BigRat->bone();                # create a +1
  $x = Math::BigRat->bone('-');             # create a -1
  $x = Math::BigRat->binf();                # create a +inf
  $x = Math::BigRat->binf('-');             # create a -inf
  $x = Math::BigRat->bnan();                # create a Not-A-Number
  $x = Math::BigRat->bpi();                 # returns pi

  $y = $x->copy();        # make a copy (unlike $y = $x)
  $y = $x->as_int();      # return as a Math::BigInt
  $y = $x->as_float();    # return as a Math::BigFloat
  $y = $x->as_rat();      # return as a Math::BigRat

  # Boolean methods (these don't modify the invocand)

  $x->is_zero();          # true if $x is 0
  $x->is_one();           # true if $x is +1
  $x->is_one("+");        # true if $x is +1
  $x->is_one("-");        # true if $x is -1
  $x->is_inf();           # true if $x is +inf or -inf
  $x->is_inf("+");        # true if $x is +inf
  $x->is_inf("-");        # true if $x is -inf
  $x->is_nan();           # true if $x is NaN

  $x->is_finite();        # true if -inf < $x < inf
  $x->is_positive();      # true if $x > 0
  $x->is_pos();           # true if $x > 0
  $x->is_negative();      # true if $x < 0
  $x->is_neg();           # true if $x < 0
  $x->is_non_positive()   # true if $x <= 0
  $x->is_non_negative()   # true if $x >= 0

  $x->is_odd();           # true if $x is odd
  $x->is_even();          # true if $x is even
  $x->is_int();           # true if $x is an integer

  # Comparison methods (these don't modify the invocand)

  $x->bcmp($y);           # compare numbers (undef, < 0, == 0, > 0)
  $x->bacmp($y);          # compare abs values (undef, < 0, == 0, > 0)
  $x->beq($y);            # true if $x == $y
  $x->bne($y);            # true if $x != $y
  $x->blt($y);            # true if $x < $y
  $x->ble($y);            # true if $x <= $y
  $x->bgt($y);            # true if $x > $y
  $x->bge($y);            # true if $x >= $y

  # Arithmetic methods (these modify the invocand)

  $x->bneg();             # negation
  $x->babs();             # absolute value
  $x->bsgn();             # sign function (-1, 0, 1, or NaN)
  $x->bdigitsum();        # sum of decimal digits
  $x->binc();             # increment $x by 1
  $x->bdec();             # decrement $x by 1
  $x->badd($y);           # addition (add $y to $x)
  $x->bsub($y);           # subtraction (subtract $y from $x)
  $x->bmul($y);           # multiplication (multiply $x by $y)
  $x->bmuladd($y, $z);    # $x = $x * $y + $z
  $x->bdiv($y);           # division (floored)
  $x->bmod($y);           # modulus (x % y)
  $x->bmodinv($mod);      # modular multiplicative inverse
  $x->bmodpow($y, $mod);  # modular exponentiation (($x ** $y) % $mod)
  $x->btdiv($y);          # division (truncated), set $x to quotient
  $x->btmod($y);          # modulus (truncated)
  $x->binv()              # inverse (1/$x)
  $x->bpow($y);           # power of arguments (x ** y)
  $x->blog();             # logarithm of $x to base e (Euler's number)
  $x->blog($base);        # logarithm of $x to base $base (e.g., base 2)
  $x->bexp();             # calculate e ** $x where e is Euler's number
  $x->bilog2();           # log2($x) rounded down to nearest int
  $x->bilog10();          # log10($x) rounded down to nearest int
  $x->bclog2();           # log2($x) rounded up to nearest int
  $x->bclog10();          # log10($x) rounded up to nearest int
  $x->bnok($y);           # combinations (binomial coefficient n over k)
  $x->bperm($y);          # permutations
  $x->buparrow($n, $y);   # Knuth's up-arrow notation
  $x->bhyperop($n, $y);   # n'th hyperoprator
  $x->backermann($y);     # the Ackermann function
  $x->bsin();             # sine
  $x->bcos();             # cosine
  $x->batan();            # inverse tangent
  $x->batan2($y);         # two-argument inverse tangent
  $x->bsqrt();            # calculate square root
  $x->broot($y);          # $y'th root of $x (e.g. $y == 3 => cubic root)
  $x->bfac();             # factorial of $x (1*2*3*4*..$x)
  $x->bdfac();            # double factorial of $x ($x*($x-2)*($x-4)*...)
  $x->btfac();            # triple factorial of $x ($x*($x-3)*($x-6)*...)
  $x->bmfac($k);          # $k'th multi-factorial of $x ($x*($x-$k)*...)
  $x->bfib($k);           # $k'th Fibonacci number
  $x->blucas($k);         # $k'th Lucas number

  $x->blsft($n);          # left shift $n places in base 2
  $x->blsft($n, $b);      # left shift $n places in base $b
  $x->brsft($n);          # right shift $n places in base 2
  $x->brsft($n, $b);      # right shift $n places in base $b

  # Bitwise methods (these modify the invocand)

  $x->bblsft($y);         # bitwise left shift
  $x->bbrsft($y);         # bitwise right shift
  $x->band($y);           # bitwise and
  $x->bior($y);           # bitwise inclusive or
  $x->bxor($y);           # bitwise exclusive or
  $x->bnot();             # bitwise not (two's complement)

  # Rounding methods (these modify the invocand)

  $x->round($A, $P, $R);  # round to accuracy or precision using
                          #   rounding mode $R
  $x->bround($n);         # accuracy: preserve $n digits
  $x->bfround($n);        # $n > 0: round to $nth digit left of dec. point
                          # $n < 0: round to $nth digit right of dec. point
  $x->bfloor();           # round towards minus infinity
  $x->bceil();            # round towards plus infinity
  $x->bint();             # round towards zero

  # Other mathematical methods (these don't modify the invocand)

  $x->bgcd($y);           # greatest common divisor
  $x->blcm($y);           # least common multiple

  # Object property methods (these don't modify the invocand)

  $x->sign();             # the sign, either +, - or NaN
  $x->digit($n);          # the nth digit, counting from the right
  $x->digit(-$n);         # the nth digit, counting from the left
  $x->digitsum();         # sum of decimal digits
  $x->length();           # return number of digits in number
  $x->mantissa();         # return (signed) mantissa as a Math::BigInt
  $x->exponent();         # return exponent as a Math::BigInt
  $x->parts();            # return (mantissa,exponent) as a Math::BigInt
  $x->sparts();           # mantissa and exponent (as integers)
  $x->nparts();           # mantissa and exponent (normalised)
  $x->eparts();           # mantissa and exponent (engineering notation)
  $x->dparts();           # integer and fraction part
  $x->fparts();           # numerator and denominator
  $x->numerator();        # numerator
  $x->denominator();      # denominator

  # Conversion methods (these don't modify the invocand)

  $x->bstr();             # decimal notation (possibly zero padded)
  $x->bnstr();            # string in normalized notation
  $x->bestr();            # string in engineering notation
  $x->bdstr();            # string in decimal notation (no padding)
  $x->bfstr();            # string in fractional notation

  $x->to_hex();           # as signed hexadecimal string
  $x->to_bin();           # as signed binary string
  $x->to_oct();           # as signed octal string
  $x->to_bytes();         # as byte string
  $x->to_base($b);        # as string in any base
  $x->to_base_num($b);    # as array of integers in any base
  $x->to_ieee754($fmt);   # to bytes encoded according to IEEE 754-2008

  $x->as_hex();           # as signed hexadecimal string with "0x" prefix
  $x->as_bin();           # as signed binary string with "0b" prefix
  $x->as_oct();           # as signed octal string with "0" prefix

  # Other conversion methods (these don't modify the invocand)

  $x->numify();           # return as scalar (might overflow or underflow)

=head1 DESCRIPTION

Math::BigRat complements L<Math::BigInt> and L<Math::BigFloat> by providing
support for arbitrary big rational numbers.

=head2 Math Library

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

=item from_dec()

    my $h = Math::BigRat->from_dec("1.2");

Create a BigRat from a decimal number in string form. It is equivalent to
L</new()>, but does not accept anything but strings representing finite,
decimal numbers.

=item from_hex()

    my $h = Math::BigRat->from_hex("0x10");

Create a BigRat from a hexadecimal number in string form.

=item from_oct()

    my $o = Math::BigRat->from_oct("020");

Create a BigRat from an octal number in string form.

=item from_bin()

    my $b = Math::BigRat->from_bin("0b10000000");

Create a BigRat from an binary number in string form.

=item from_bytes()

    $x = Math::BigRat->from_bytes("\xf3\x6b");  # $x = 62315

Interpret the input as a byte string, assuming big endian byte order. The
output is always a non-negative, finite integer.

See L<Math::BigInt/from_bytes()>.

=item from_ieee754()

    # set $x to 13176795/4194304, the closest value to pi that can be
    # represented in the binary32 (single) format
    $x = Math::BigRat -> from_ieee754("40490fdb", "binary32");

Interpret the input as a value encoded as described in IEEE754-2008.

See L<Math::BigFloat/from_ieee754()>.

=item from_base()

See L<Math::BigInt/from_base()>.

=item bzero()

    $x = Math::BigRat->bzero();

Creates a new BigRat object representing zero.
If used on an object, it will set it to zero:

    $x->bzero();

=item bone()

    $x = Math::BigRat->bone($sign);

Creates a new BigRat object representing one. The optional argument is
either '-' or '+', indicating whether you want one or minus one.
If used on an object, it will set it to one:

    $x->bone();                 # +1
    $x->bone('-');              # -1

=item binf()

    $x = Math::BigRat->binf($sign);

Creates a new BigRat object representing infinity. The optional argument is
either '-' or '+', indicating whether you want infinity or minus infinity.
If used on an object, it will set it to infinity:

    $x->binf();
    $x->binf('-');

=item bnan()

    $x = Math::BigRat->bnan();

Creates a new BigRat object representing NaN (Not A Number).
If used on an object, it will set it to NaN:

    $x->bnan();

=item bpi()

    $x = Math::BigRat -> bpi();         # default accuracy
    $x = Math::BigRat -> bpi(7);        # specified accuracy

Returns a rational approximation of PI accurate to the specified accuracy or
the default accuracy if no accuracy is specified. If called as an instance
method, the value is assigned to the invocand.

    $x = Math::BigRat -> bpi(1);        # returns "3"
    $x = Math::BigRat -> bpi(3);        # returns "22/7"
    $x = Math::BigRat -> bpi(7);        # returns "355/113"

=item copy()

    my $z = $x->copy();

Makes a deep copy of the object.

Please see the documentation in L<Math::BigInt> for further details.

=item as_int()

    $y = $x -> as_int();        # $y is a Math::BigInt

Returns $x as a Math::BigInt object regardless of upgrading and downgrading. If
$x is finite, but not an integer, $x is truncated.

=item as_rat()

    $y = $x -> as_rat();        # $y is a Math::BigRat

Returns $x a Math::BigRat object regardless of upgrading and downgrading. The
invocand is not modified.

=item as_float()

    $x = Math::BigRat->new('13/7');
    print $x->as_float(), "\n";             # '1'

    $x = Math::BigRat->new('2/3');
    print $x->as_float(5), "\n";            # '0.66667'

Returns a copy of the object as Math::BigFloat object regardless of upgrading
and downgrading, preserving the accuracy as wanted, or the default of 40
digits.

=item bround()/round()/bfround()

Are not yet implemented.

=item is_zero()

    print "$x is 0\n" if $x->is_zero();

Return true if $x is exactly zero, otherwise false.

=item is_one()

    print "$x is 1\n" if $x->is_one();

Return true if $x is exactly one, otherwise false.

=item is_finite()

    $x->is_finite();    # true if $x is not +inf, -inf or NaN

Returns true if the invocand is a finite number, i.e., it is neither +inf,
-inf, nor NaN.

=item is_positive()

=item is_pos()

    print "$x is >= 0\n" if $x->is_positive();

Return true if $x is positive (greater than or equal to zero), otherwise
false. Please note that '+inf' is also positive, while 'NaN' and '-inf' aren't.

L</is_positive()> is an alias for L</is_pos()>.

=item is_negative()

=item is_neg()

    print "$x is < 0\n" if $x->is_negative();

Return true if $x is negative (smaller than zero), otherwise false. Please
note that '-inf' is also negative, while 'NaN' and '+inf' aren't.

L</is_negative()> is an alias for L</is_neg()>.

=item is_odd()

    print "$x is odd\n" if $x->is_odd();

Return true if $x is odd, otherwise false.

=item is_even()

    print "$x is even\n" if $x->is_even();

Return true if $x is even, otherwise false.

=item is_int()

    print "$x is an integer\n" if $x->is_int();

Return true if $x has a denominator of 1 (e.g. no fraction parts), otherwise
false. Please note that '-inf', 'inf' and 'NaN' aren't integer.

=back

=head2 Comparison methods

None of these methods modify the invocand object. Note that a C<NaN> is neither
less than, greater than, or equal to anything else, even a C<NaN>.

=over

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

=item bneg()

    $x->bneg();

Used to negate the object in-place.

=item bnorm()

    $x->bnorm();

Reduce the number to the shortest form. This routine is called
automatically whenever it is needed.

=item binc()

    $x->binc();

Increments $x by 1 and returns the result.

=item bdec()

    $x->bdec();

Decrements $x by 1 and returns the result.

=item badd()

    $x->badd($y);

Adds $y to $x and returns the result.

=item bsub()

    $x->bsub($y);

Subtracts $y from $x and returns the result.

=item bmul()

    $x->bmul($y);

Multiplies $y to $x and returns the result.

=item bdiv()

    $q = $x->bdiv($y);
    ($q, $r) = $x->bdiv($y);

In scalar context, divides $x by $y and returns the result. In list context,
does floored division (F-division), returning an integer $q and a remainder $r
so that $x = $q * $y + $r. The remainer (modulo) is equal to what is returned
by C<< $x->bmod($y) >>.

=item bmod()

    $x->bmod($y);

Returns $x modulo $y. When $x is finite, and $y is finite and non-zero, the
result is identical to the remainder after floored division (F-division). If,
in addition, both $x and $y are integers, the result is identical to the result
from Perl's % operator.

=item binv()

    $x->binv();

Inverse of $x.

=item bsqrt()

    $x->bsqrt();

Calculate the square root of $x.

=item bpow()

    $x->bpow($y);

Compute $x ** $y.

Please see the documentation in L<Math::BigInt> for further details.

=item broot()

    $x->broot($n);

Calculate the N'th root of $x.

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

=item blog()

    $x->blog($base, $accuracy);         # logarithm of x to the base $base

If C<$base> is not defined, Euler's number (e) is used:

    print $x->blog(undef, 100);         # log(x) to 100 digits

=item bexp()

    $x->bexp($accuracy);        # calculate e ** X

Calculates two integers A and B so that A/B is equal to C<e ** $x>, where C<e> is
Euler's number.

This method was added in v0.20 of Math::BigRat (May 2007).

See also L</blog()>.

=item bnok()

See L<Math::BigInt/bnok()>.

=item bperm()

See L<Math::BigInt/bperm()>.

=item bfac()

    $x->bfac();

Calculates the factorial of $x. For instance:

    print Math::BigRat->new('3/1')->bfac(), "\n";   # 1*2*3
    print Math::BigRat->new('5/1')->bfac(), "\n";   # 1*2*3*4*5

Works currently only for integers.

=item band()

    $x->band($y);               # bitwise and

=item bior()

    $x->bior($y);               # bitwise inclusive or

=item bxor()

    $x->bxor($y);               # bitwise exclusive or

=item bnot()

    $x->bnot();                 # bitwise not (two's complement)

=item bfloor()

    $x->bfloor();

Round $x towards minus infinity, i.e., set $x to the largest integer less than
or equal to $x.

=item bceil()

    $x->bceil();

Round $x towards plus infinity, i.e., set $x to the smallest integer greater
than or equal to $x.

=item bint()

    $x->bint();

Round $x towards zero.

=item bgcd()

    $x -> bgcd($y);             # GCD of $x and $y
    $x -> bgcd($y, $z, ...);    # GCD of $x, $y, $z, ...

Returns the greatest common divisor (GCD), which is the number with the largest
absolute value such that $x/$gcd, $y/$gcd, ... is an integer. For example, when
the operands are 4/5 and 6/5, the GCD is 2/5. This is a generalisation of the
ordinary GCD for integers. See L<Math::BigInt/gcd()>.

=item digit()

    print Math::BigRat->new('123/1')->digit(1);     # 1
    print Math::BigRat->new('123/1')->digit(-1);    # 3

Return the N'ths digit from X when X is an integer value.

=item length()

    $len = $x->length();

Return the length of $x in digits for integer values.

=item parts()

    ($n, $d) = $x->parts();

Return a list consisting of (signed) numerator and (unsigned) denominator as
BigInts.

=item dparts()

Returns the integer part and the fraction part.

=item fparts()

Returns the smallest possible numerator and denominator so that the numerator
divided by the denominator gives back the original value. For finite numbers,
both values are integers. Mnemonic: fraction.

=item numerator()

    $n = $x->numerator();

Returns a copy of the numerator (the part above the line) as signed BigInt.

=item denominator()

    $d = $x->denominator();

Returns a copy of the denominator (the part under the line) as positive BigInt.

=back

=head2 String conversion methods

=over

=item bstr()

    my $x = Math::BigRat->new('8/4');
    print $x->bstr(), "\n";             # prints 1/2

Returns a string representing the number.

=item bnstr()

See L<Math::BigInt/bnstr()>.

=item bestr()

See L<Math::BigInt/bestr()>.

=item bdstr()

See L<Math::BigInt/bdstr()>.

=item to_bytes()

See L<Math::BigInt/to_bytes()>.

=item to_ieee754()

See L<Math::BigFloat/to_ieee754()>.

=item as_hex()

    $x = Math::BigRat->new('13');
    print $x->as_hex(), "\n";               # '0xd'

Returns the BigRat as hexadecimal string. Works only for integers.

=item as_oct()

    $x = Math::BigRat->new('13');
    print $x->as_oct(), "\n";               # '015'

Returns the BigRat as octal string. Works only for integers.

=item as_bin()

    $x = Math::BigRat->new('13');
    print $x->as_bin(), "\n";               # '0x1101'

Returns the BigRat as binary string. Works only for integers.

=item numify()

    my $y = $x->numify();

Returns the object as a scalar. This will lose some data if the object
cannot be represented by a normal Perl scalar (integer or float), so
use L</as_int()> or L</as_float()> instead.

This routine is automatically used whenever a scalar is required:

    my $x = Math::BigRat->new('3/1');
    @array = (0, 1, 2, 3);
    $y = $array[$x];                # set $y to 3

=item config()

    Math::BigRat->config("trap_nan" => 1);      # set
    $accu = Math::BigRat->config("accuracy");   # get

Set or get configuration parameter values. Read-only parameters are marked as
RO. Read-write parameters are marked as RW. The following parameters are
supported.

    Parameter       RO/RW   Description
                            Example
    ============================================================
    lib             RO      Name of the math backend library
                            Math::BigInt::Calc
    lib_version     RO      Version of the math backend library
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
    div_scale       RW      Fallback accuracy for div, sqrt etc.
                            40
    trap_nan        RW      Trap NaNs
                            undef
    trap_inf        RW      Trap +inf/-inf
                            undef

=back

=head1 NUMERIC LITERALS

After C<use Math::BigRat ':constant'> all numeric literals in the given scope
are converted to C<Math::BigRat> objects. This conversion happens at compile
time. Every non-integer is convert to a NaN.

For example,

    perl -MMath::BigRat=:constant -le 'print 2**150'

prints the exact value of C<2**150>. Note that without conversion of constants
to objects the expression C<2**150> is calculated using Perl scalars, which
leads to an inaccurate result.

Please note that strings are not affected, so that

    use Math::BigRat qw/:constant/;

    $x = "1234567890123456789012345678901234567890"
            + "123456789123456789";

does give you what you expect. You need an explicit Math::BigRat->new() around
at least one of the operands. You should also quote large constants to prevent
loss of precision:

    use Math::BigRat;

    $x = Math::BigRat->new("1234567889123456789123456789123456789");

Without the quotes Perl first converts the large number to a floating point
constant at compile time, and then converts the result to a Math::BigRat object
at run time, which results in an inaccurate result.

=head2 Hexadecimal, octal, and binary floating point literals

Perl (and this module) accepts hexadecimal, octal, and binary floating point
literals, but use them with care with Perl versions before v5.32.0, because some
versions of Perl silently give the wrong result. Below are some examples of
different ways to write the number decimal 314.

Hexadecimal floating point literals:

    0x1.3ap+8         0X1.3AP+8
    0x1.3ap8          0X1.3AP8
    0x13a0p-4         0X13A0P-4

Octal floating point literals (with "0" prefix):

    01.164p+8         01.164P+8
    01.164p8          01.164P8
    011640p-4         011640P-4

Octal floating point literals (with "0o" prefix) (requires v5.34.0):

    0o1.164p+8        0O1.164P+8
    0o1.164p8         0O1.164P8
    0o11640p-4        0O11640P-4

Binary floating point literals:

    0b1.0011101p+8    0B1.0011101P+8
    0b1.0011101p8     0B1.0011101P8
    0b10011101000p-2  0B10011101000P-2

=head1 BUGS

Please report any bugs or feature requests to
C<bug-math-bigint at rt.cpan.org>, or through the web interface at
L<https://rt.cpan.org/Ticket/Create.html?Queue=Math-BigInt> (requires login).
We will be notified, and then you'll automatically be notified of progress on
your bug as I make changes.

=head1 SUPPORT

You can find documentation for this module with the perldoc command.

    perldoc Math::BigInt

You can also look for information at:

=over 4

=item * GitHub

L<https://github.com/pjacklam/p5-Math-BigInt>

=item * RT: CPAN's request tracker

L<https://rt.cpan.org/Dist/Display.html?Name=Math-BigInt>

=item * MetaCPAN

L<https://metacpan.org/release/Math-BigInt>

=item * CPAN Testers Matrix

L<http://matrix.cpantesters.org/?dist=Math-BigInt>

=back

=head1 LICENSE

This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=head1 SEE ALSO

L<Math::BigInt> and L<Math::BigFloat> as well as the backend libraries
L<Math::BigInt::FastCalc>, L<Math::BigInt::GMP>, and L<Math::BigInt::Pari>,
L<Math::BigInt::GMPz>, and L<Math::BigInt::BitVect>.

The pragmas L<bigint>, L<bigfloat>, and L<bigrat> might also be of interest. In
addition there is the L<bignum> pragma which does upgrading and downgrading.

=head1 AUTHORS

=over 4

=item *

Tels L<http://bloodgate.com/> 2001-2009.

=item *

Maintained by Peter John Acklam <pjacklam@gmail.com> 2011-

=back

=cut
