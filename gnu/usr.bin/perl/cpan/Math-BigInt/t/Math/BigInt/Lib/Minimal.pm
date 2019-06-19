# This is a rather minimalistic library, whose purpose is to test inheritance
# from its parent class.

package Math::BigInt::Lib::Minimal;

use 5.006001;
use strict;
use warnings;

use Carp;
use Math::BigInt::Lib;

our @ISA = ('Math::BigInt::Lib');

#my $BASE_LEN = 4;
my $BASE_LEN = 9;
my $BASE     = 0 + ("1" . ("0" x $BASE_LEN));
my $MAX_VAL  = $BASE - 1;

# Do we need api_version() at all, now that we have a virtual parent class that
# will provide any missing methods? Fixme!

sub api_version () { 2; }

sub _new {
    my ($class, $str) = @_;
    croak "Invalid input string '$str'" unless $str =~ /^([1-9]\d*|0)\z/;

    my $n = length $str;
    my $p = int($n / $BASE_LEN);
    my $q = $n % $BASE_LEN;

    my $format = $] < 5.9008 ? "a$BASE_LEN" x $p
                             : "(a$BASE_LEN)*";
    $format = "a$q" . $format if $q > 0;

    my $self = [ reverse(map { 0 + $_ } unpack($format, $str)) ];
    return bless $self, $class;
}

##############################################################################
# convert back to string and number

sub _str {
    my ($class, $x) = @_;
    my $idx = $#$x;             # index of last element

    # Handle first one differently, since it should not have any leading zeros.

    my $str = int($x->[$idx]);

    if ($idx > 0) {
        my $z = '0' x ($BASE_LEN - 1);
        while (--$idx >= 0) {
            $str .= substr($z . $x->[$idx], -$BASE_LEN);
        }
    }
    $str;
}

##############################################################################
# actual math code

sub _add {
    # (ref to int_num_array, ref to int_num_array)
    #
    # Routine to add two base 1eX numbers stolen from Knuth Vol 2 Algorithm A
    # pg 231. There are separate routines to add and sub as per Knuth pg 233.
    # This routine modifies array x, but not y.

    my ($c, $x, $y) = @_;

    # $x + 0 => $x

    return $x if @$y == 1 && $y->[0] == 0;

    # 0 + $y => $y->copy

    if (@$x == 1 && $x->[0] == 0) {
        @$x = @$y;
        return $x;
    }

    # For each in Y, add Y to X and carry. If after that, something is left in
    # X, foreach in X add carry to X and then return X, carry. Trades one
    # "$j++" for having to shift arrays.

    my $i;
    my $car = 0;
    my $j = 0;
    for $i (@$y) {
        $x->[$j] -= $BASE if $car = (($x->[$j] += $i + $car) >= $BASE) ? 1 : 0;
        $j++;
    }
    while ($car != 0) {
        $x->[$j] -= $BASE if $car = (($x->[$j] += $car) >= $BASE) ? 1 : 0;
        $j++;
    }

    $x;
}

sub _sub {
    # (ref to int_num_array, ref to int_num_array, swap)
    #
    # Subtract base 1eX numbers -- stolen from Knuth Vol 2 pg 232, $x > $y
    # subtract Y from X by modifying x in place
    my ($c, $sx, $sy, $s) = @_;

    my $car = 0;
    my $i;
    my $j = 0;
    if (!$s) {
        for $i (@$sx) {
            last unless defined $sy->[$j] || $car;
            $i += $BASE if $car = (($i -= ($sy->[$j] || 0) + $car) < 0);
            $j++;
        }
        # might leave leading zeros, so fix that
        return __strip_zeros($sx);
    }
    for $i (@$sx) {
        # We can't do an early out if $x < $y, since we need to copy the high
        # chunks from $y. Found by Bob Mathews.
        #last unless defined $sy->[$j] || $car;
        $sy->[$j] += $BASE
          if $car = ($sy->[$j] = $i - ($sy->[$j] || 0) - $car) < 0;
        $j++;
    }
    # might leave leading zeros, so fix that
    __strip_zeros($sy);
}

# The following _mul function is an exact copy of _mul_use_div_64 in
# Math::BigInt::Calc.

sub _mul {
    # (ref to int_num_array, ref to int_num_array)
    # multiply two numbers in internal representation
    # modifies first arg, second need not be different from first
    # works for 64 bit integer with "use integer"
    my ($c, $xv, $yv) = @_;

    use integer;
    if (@$yv == 1) {
        # shortcut for two small numbers, also handles $x == 0
        if (@$xv == 1) {
            # shortcut for two very short numbers (improved by Nathan Zook)
            # works also if xv and yv are the same reference, and handles also $x == 0
            if (($xv->[0] *= $yv->[0]) >= $BASE) {
                $xv->[0] =
                  $xv->[0] - ($xv->[1] = $xv->[0] / $BASE) * $BASE;
            }
            return $xv;
        }
        # $x * 0 => 0
        if ($yv->[0] == 0) {
            @$xv = (0);
            return $xv;
        }
        # multiply a large number a by a single element one, so speed up
        my $y = $yv->[0];
        my $car = 0;
        foreach my $i (@$xv) {
            #$i = $i * $y + $car; $car = $i / $BASE; $i -= $car * $BASE;
            $i = $i * $y + $car;
            $i -= ($car = $i / $BASE) * $BASE;
        }
        push @$xv, $car if $car != 0;
        return $xv;
    }
    # shortcut for result $x == 0 => result = 0
    return $xv if ( ((@$xv == 1) && ($xv->[0] == 0)) );

    # since multiplying $x with $x fails, make copy in this case
    $yv = $c->_copy($xv) if $xv == $yv; # same references?

    my @prod = ();
    my ($prod, $car, $cty, $xi, $yi);
    for $xi (@$xv) {
        $car = 0;
        $cty = 0;
        # looping through this if $xi == 0 is silly - so optimize it away!
        $xi = (shift @prod || 0), next if $xi == 0;
        for $yi (@$yv) {
            $prod = $xi * $yi + ($prod[$cty] || 0) + $car;
            $prod[$cty++] = $prod - ($car = $prod / $BASE) * $BASE;
        }
        $prod[$cty] += $car if $car; # need really to check for 0?
        $xi = shift @prod || 0;      # || 0 makes v5.005_3 happy
    }
    push @$xv, @prod;
    $xv;
}

# The following _div function is an exact copy of _div_use_div_64 in
# Math::BigInt::Calc.

sub _div {
    # ref to array, ref to array, modify first array and return remainder if
    # in list context
    # This version works on 64 bit integers
    my ($c, $x, $yorg) = @_;

    use integer;
    # the general div algorithm here is about O(N*N) and thus quite slow, so
    # we first check for some special cases and use shortcuts to handle them.

    # This works, because we store the numbers in a chunked format where each
    # element contains 5..7 digits (depending on system).

    # if both numbers have only one element:
    if (@$x == 1 && @$yorg == 1) {
        # shortcut, $yorg and $x are two small numbers
        if (wantarray) {
            my $rem = [ $x->[0] % $yorg->[0] ];
            bless $rem, $c;
            $x->[0] = int($x->[0] / $yorg->[0]);
            return ($x, $rem);
        } else {
            $x->[0] = int($x->[0] / $yorg->[0]);
            return $x;
        }
    }
    # if x has more than one, but y has only one element:
    if (@$yorg == 1) {
        my $rem;
        $rem = $c->_mod($c->_copy($x), $yorg) if wantarray;

        # shortcut, $y is < $BASE
        my $j = @$x;
        my $r = 0;
        my $y = $yorg->[0];
        my $b;
        while ($j-- > 0) {
            $b = $r * $BASE + $x->[$j];
            $x->[$j] = int($b/$y);
            $r = $b % $y;
        }
        pop @$x if @$x > 1 && $x->[-1] == 0; # splice up a leading zero
        return ($x, $rem) if wantarray;
        return $x;
    }
    # now x and y have more than one element

    # check whether y has more elements than x, if yet, the result will be 0
    if (@$yorg > @$x) {
        my $rem;
        $rem = $c->_copy($x) if wantarray;    # make copy
        @$x = 0;                        # set to 0
        return ($x, $rem) if wantarray; # including remainder?
        return $x;                      # only x, which is [0] now
    }
    # check whether the numbers have the same number of elements, in that case
    # the result will fit into one element and can be computed efficiently
    if (@$yorg == @$x) {
        my $rem;
        # if $yorg has more digits than $x (it's leading element is longer than
        # the one from $x), the result will also be 0:
        if (length(int($yorg->[-1])) > length(int($x->[-1]))) {
            $rem = $c->_copy($x) if wantarray;     # make copy
            @$x = 0;                          # set to 0
            return ($x, $rem) if wantarray; # including remainder?
            return $x;
        }
        # now calculate $x / $yorg

        if (length(int($yorg->[-1])) == length(int($x->[-1]))) {
            # same length, so make full compare

            my $a = 0;
            my $j = @$x - 1;
            # manual way (abort if unequal, good for early ne)
            while ($j >= 0) {
                last if ($a = $x->[$j] - $yorg->[$j]);
                $j--;
            }
            # $a contains the result of the compare between X and Y
            # a < 0: x < y, a == 0: x == y, a > 0: x > y
            if ($a <= 0) {
                $rem = $c->_zero();                  # a = 0 => x == y => rem 0
                $rem = $c->_copy($x) if $a != 0;       # a < 0 => x < y => rem = x
                @$x = 0;                       # if $a < 0
                $x->[0] = 1 if $a == 0;        # $x == $y
                return ($x, $rem) if wantarray; # including remainder?
                return $x;
            }
            # $x >= $y, so proceed normally
        }
    }

    # all other cases:

    my $y = $c->_copy($yorg);         # always make copy to preserve

    my ($car, $bar, $prd, $dd, $xi, $yi, @q, $v2, $v1, @d, $tmp, $q, $u2, $u1, $u0);

    $car = $bar = $prd = 0;
    if (($dd = int($BASE / ($y->[-1] + 1))) != 1) {
        for $xi (@$x) {
            $xi = $xi * $dd + $car;
            $xi -= ($car = int($xi / $BASE)) * $BASE;
        }
        push(@$x, $car);
        $car = 0;
        for $yi (@$y) {
            $yi = $yi * $dd + $car;
            $yi -= ($car = int($yi / $BASE)) * $BASE;
        }
    } else {
        push(@$x, 0);
    }

    # @q will accumulate the final result, $q contains the current computed
    # part of the final result

    @q = ();
    ($v2, $v1) = @$y[-2, -1];
    $v2 = 0 unless $v2;
    while ($#$x > $#$y) {
        ($u2, $u1, $u0) = @$x[-3..-1];
        $u2 = 0 unless $u2;
        #warn "oups v1 is 0, u0: $u0 $y->[-2] $y->[-1] l ",scalar @$y,"\n"
        # if $v1 == 0;
        $q = (($u0 == $v1) ? $MAX_VAL : int(($u0 * $BASE + $u1) / $v1));
        --$q while ($v2 * $q > ($u0 * $BASE +$ u1- $q*$v1) * $BASE + $u2);
        if ($q) {
            ($car, $bar) = (0, 0);
            for ($yi = 0, $xi = $#$x - $#$y - 1; $yi <= $#$y; ++$yi, ++$xi) {
                $prd = $q * $y->[$yi] + $car;
                $prd -= ($car = int($prd / $BASE)) * $BASE;
                $x->[$xi] += $BASE if ($bar = (($x->[$xi] -= $prd + $bar) < 0));
            }
            if ($x->[-1] < $car + $bar) {
                $car = 0;
                --$q;
                for ($yi = 0, $xi = $#$x - $#$y - 1; $yi <= $#$y; ++$yi, ++$xi) {
                    $x->[$xi] -= $BASE
                      if ($car = (($x->[$xi] += $y->[$yi] + $car) >= $BASE));
                }
            }
        }
        pop(@$x);
        unshift(@q, $q);
    }
    if (wantarray) {
        my $d = bless [], $c;
        if ($dd != 1) {
            $car = 0;
            for $xi (reverse @$x) {
                $prd = $car * $BASE + $xi;
                $car = $prd - ($tmp = int($prd / $dd)) * $dd;
                unshift(@$d, $tmp);
            }
        } else {
            @$d = @$x;
        }
        @$x = @q;
        __strip_zeros($x);
        __strip_zeros($d);
        return ($x, $d);
    }
    @$x = @q;
    __strip_zeros($x);
    $x;
}

# The following _mod function is an exact copy of _mod in Math::BigInt::Calc.

sub _mod {
    # if possible, use mod shortcut
    my ($c, $x, $yo) = @_;

    # slow way since $y too big
    if (@$yo > 1) {
        my ($xo, $rem) = $c->_div($x, $yo);
        @$x = @$rem;
        return $x;
    }

    my $y = $yo->[0];

    # if both are single element arrays
    if (@$x == 1) {
        $x->[0] %= $y;
        return $x;
    }

    # if @$x has more than one element, but @$y is a single element
    my $b = $BASE % $y;
    if ($b == 0) {
        # when BASE % Y == 0 then (B * BASE) % Y == 0
        # (B * BASE) % $y + A % Y => A % Y
        # so need to consider only last element: O(1)
        $x->[0] %= $y;
    } elsif ($b == 1) {
        # else need to go through all elements in @$x: O(N), but loop is a bit
        # simplified
        my $r = 0;
        foreach (@$x) {
            $r = ($r + $_) % $y; # not much faster, but heh...
            #$r += $_ % $y; $r %= $y;
        }
        $r = 0 if $r == $y;
        $x->[0] = $r;
    } else {
        # else need to go through all elements in @$x: O(N)
        my $r = 0;
        my $bm = 1;
        foreach (@$x) {
            $r = ($_ * $bm + $r) % $y;
            $bm = ($bm * $b) % $y;

            #$r += ($_ % $y) * $bm;
            #$bm *= $b;
            #$bm %= $y;
            #$r %= $y;
        }
        $r = 0 if $r == $y;
        $x->[0] = $r;
    }
    @$x = $x->[0];              # keep one element of @$x
    return $x;
}

sub __strip_zeros {
    # Internal normalization function that strips leading zeros from the array.
    # Args: ref to array
    my $x = shift;

    push @$x, 0 if @$x == 0;    # div might return empty results, so fix it
    return $x if @$x == 1;      # early out

    #print "strip: cnt $cnt i $i\n";
    # '0', '3', '4', '0', '0',
    #  0    1    2    3    4
    # cnt = 5, i = 4
    # i = 4
    # i = 3
    # => fcnt = cnt - i (5-2 => 3, cnt => 5-1 = 4, throw away from 4th pos)
    # >= 1: skip first part (this can be zero)

    my $i = $#$x;
    while ($i > 0) {
        last if $x->[$i] != 0;
        $i--;
    }
    $i++;
    splice(@$x, $i) if $i < @$x;
    $x;
}

###############################################################################
# check routine to test internal state for corruptions

sub _check {
    # used by the test suite
    my ($class, $x) = @_;

    return "Undefined" unless defined $x;
    return "$x is not a reference" unless ref($x);
    return "Not an '$class'" unless ref($x) eq $class;

    for (my $i = 0 ; $i <= $#$x ; ++ $i) {
        my $e = $x -> [$i];

        return "Element at index $i is undefined"
          unless defined $e;

        return "Element at index $i is a '" . ref($e) .
          "', which is not a scalar"
          unless ref($e) eq "";

        return "Element at index $i is '$e', which does not look like an" .
          " normal integer"
            #unless $e =~ /^([1-9]\d*|0)\z/;
            unless $e =~ /^\d+\z/;

        return "Element at index $i is '$e', which is negative"
          if $e < 0;

        return "Element at index $i is '$e', which is not smaller than" .
          " the base '$BASE'"
            if $e >= $BASE;

        return "Element at index $i (last element) is zero"
          if $#$x > 0 && $i == $#$x && $e == 0;
    }

    return 0;
}

##############################################################################
##############################################################################

1;

__END__

=pod

=head1 NAME

Math::BigInt::Calc - Pure Perl module to support Math::BigInt

=head1 SYNOPSIS

This library provides support for big integer calculations. It is not
intended to be used by other modules. Other modules which support the same
API (see below) can also be used to support Math::BigInt, like
Math::BigInt::GMP and Math::BigInt::Pari.

=head1 DESCRIPTION

In this library, the numbers are represented in base B = 10**N, where N is
the largest possible value that does not cause overflow in the intermediate
computations. The base B elements are stored in an array, with the least
significant element stored in array element zero. There are no leading zero
elements, except a single zero element when the number is zero.

For instance, if B = 10000, the number 1234567890 is represented internally
as [3456, 7890, 12].

=head1 THE Math::BigInt API

In order to allow for multiple big integer libraries, Math::BigInt was
rewritten to use a plug-in library for core math routines. Any module which
conforms to the API can be used by Math::BigInt by using this in your program:

        use Math::BigInt lib => 'libname';

'libname' is either the long name, like 'Math::BigInt::Pari', or only the short
version, like 'Pari'.

=head2 General Notes

A library only needs to deal with unsigned big integers. Testing of input
parameter validity is done by the caller, so there is no need to worry about
underflow (e.g., in C<_sub()> and C<_dec()>) nor about division by zero (e.g.,
in C<_div()>) or similar cases.

For some methods, the first parameter can be modified. That includes the
possibility that you return a reference to a completely different object
instead. Although keeping the reference and just changing its contents is
preferred over creating and returning a different reference.

Return values are always objects, strings, Perl scalars, or true/false for
comparison routines.

=head2 API version 1

The following methods must be defined in order to support the use by
Math::BigInt v1.70 or later.

=head3 API version

=over 4

=item I<api_version()>

Return API version as a Perl scalar, 1 for Math::BigInt v1.70, 2 for
Math::BigInt v1.83.

=back

=head3 Constructors

=over 4

=item I<_new(STR)>

Convert a string representing an unsigned decimal number to an object
representing the same number. The input is normalize, i.e., it matches
C<^(0|[1-9]\d*)$>.

=item I<_zero()>

Return an object representing the number zero.

=item I<_one()>

Return an object representing the number one.

=item I<_two()>

Return an object representing the number two.

=item I<_ten()>

Return an object representing the number ten.

=item I<_from_bin(STR)>

Return an object given a string representing a binary number. The input has a
'0b' prefix and matches the regular expression C<^0[bB](0|1[01]*)$>.

=item I<_from_oct(STR)>

Return an object given a string representing an octal number. The input has a
'0' prefix and matches the regular expression C<^0[1-7]*$>.

=item I<_from_hex(STR)>

Return an object given a string representing a hexadecimal number. The input
has a '0x' prefix and matches the regular expression
C<^0x(0|[1-9a-fA-F][\da-fA-F]*)$>.

=back

=head3 Mathematical functions

Each of these methods may modify the first input argument, except I<_bgcd()>,
which shall not modify any input argument, and I<_sub()> which may modify the
second input argument.

=over 4

=item I<_add(OBJ1, OBJ2)>

Returns the result of adding OBJ2 to OBJ1.

=item I<_mul(OBJ1, OBJ2)>

Returns the result of multiplying OBJ2 and OBJ1.

=item I<_div(OBJ1, OBJ2)>

Returns the result of dividing OBJ1 by OBJ2 and truncating the result to an
integer.

=item I<_sub(OBJ1, OBJ2, FLAG)>

=item I<_sub(OBJ1, OBJ2)>

Returns the result of subtracting OBJ2 by OBJ1. If C<flag> is false or omitted,
OBJ1 might be modified. If C<flag> is true, OBJ2 might be modified.

=item I<_dec(OBJ)>

Decrement OBJ by one.

=item I<_inc(OBJ)>

Increment OBJ by one.

=item I<_mod(OBJ1, OBJ2)>

Return OBJ1 modulo OBJ2, i.e., the remainder after dividing OBJ1 by OBJ2.

=item I<_sqrt(OBJ)>

Return the square root of the object, truncated to integer.

=item I<_root(OBJ, N)>

Return Nth root of the object, truncated to int. N is E<gt>= 3.

=item I<_fac(OBJ)>

Return factorial of object (1*2*3*4*...).

=item I<_pow(OBJ1, OBJ2)>

Return OBJ1 to the power of OBJ2. By convention, 0**0 = 1.

=item I<_modinv(OBJ1, OBJ2)>

Return modular multiplicative inverse, i.e., return OBJ3 so that

    (OBJ3 * OBJ1) % OBJ2 = 1 % OBJ2

The result is returned as two arguments. If the modular multiplicative
inverse does not exist, both arguments are undefined. Otherwise, the
arguments are a number (object) and its sign ("+" or "-").

The output value, with its sign, must either be a positive value in the
range 1,2,...,OBJ2-1 or the same value subtracted OBJ2. For instance, if the
input arguments are objects representing the numbers 7 and 5, the method
must either return an object representing the number 3 and a "+" sign, since
(3*7) % 5 = 1 % 5, or an object representing the number 2 and "-" sign,
since (-2*7) % 5 = 1 % 5.

=item I<_modpow(OBJ1, OBJ2, OBJ3)>

Return modular exponentiation, (OBJ1 ** OBJ2) % OBJ3.

=item I<_rsft(OBJ, N, B)>

Shift object N digits right in base B and return the resulting object. This is
equivalent to performing integer division by B**N and discarding the remainder,
except that it might be much faster, depending on how the number is represented
internally.

For instance, if the object $obj represents the hexadecimal number 0xabcde,
then C<$obj->_rsft(2, 16)> returns an object representing the number 0xabc. The
"remainer", 0xde, is discarded and not returned.

=item I<_lsft(OBJ, N, B)>

Shift the object N digits left in base B. This is equivalent to multiplying by
B**N, except that it might be much faster, depending on how the number is
represented internally.

=item I<_log_int(OBJ, B)>

Return integer log of OBJ to base BASE. This method has two output arguments,
the OBJECT and a STATUS. The STATUS is Perl scalar; it is 1 if OBJ is the exact
result, 0 if the result was truncted to give OBJ, and undef if it is unknown
whether OBJ is the exact result.

=item I<_gcd(OBJ1, OBJ2)>

Return the greatest common divisor of OBJ1 and OBJ2.

=back

=head3 Bitwise operators

Each of these methods may modify the first input argument.

=over 4

=item I<_and(OBJ1, OBJ2)>

Return bitwise and. If necessary, the smallest number is padded with leading
zeros.

=item I<_or(OBJ1, OBJ2)>

Return bitwise or. If necessary, the smallest number is padded with leading
zeros.

=item I<_xor(OBJ1, OBJ2)>

Return bitwise exclusive or. If necessary, the smallest number is padded
with leading zeros.

=back

=head3 Boolean operators

=over 4

=item I<_is_zero(OBJ)>

Returns a true value if OBJ is zero, and false value otherwise.

=item I<_is_one(OBJ)>

Returns a true value if OBJ is one, and false value otherwise.

=item I<_is_two(OBJ)>

Returns a true value if OBJ is two, and false value otherwise.

=item I<_is_ten(OBJ)>

Returns a true value if OBJ is ten, and false value otherwise.

=item I<_is_even(OBJ)>

Return a true value if OBJ is an even integer, and a false value otherwise.

=item I<_is_odd(OBJ)>

Return a true value if OBJ is an even integer, and a false value otherwise.

=item I<_acmp(OBJ1, OBJ2)>

Compare OBJ1 and OBJ2 and return -1, 0, or 1, if OBJ1 is less than, equal
to, or larger than OBJ2, respectively.

=back

=head3 String conversion

=over 4

=item I<_str(OBJ)>

Return a string representing the object. The returned string should have no
leading zeros, i.e., it should match C<^(0|[1-9]\d*)$>.

=item I<_as_bin(OBJ)>

Return the binary string representation of the number. The string must have a
'0b' prefix.

=item I<_as_oct(OBJ)>

Return the octal string representation of the number. The string must have
a '0x' prefix.

Note: This method was required from Math::BigInt version 1.78, but the required
API version number was not incremented, so there are older libraries that
support API version 1, but do not support C<_as_oct()>.

=item I<_as_hex(OBJ)>

Return the hexadecimal string representation of the number. The string must
have a '0x' prefix.

=back

=head3 Numeric conversion

=over 4

=item I<_num(OBJ)>

Given an object, return a Perl scalar number (int/float) representing this
number.

=back

=head3 Miscellaneous

=over 4

=item I<_copy(OBJ)>

Return a true copy of the object.

=item I<_len(OBJ)>

Returns the number of the decimal digits in the number. The output is a
Perl scalar.

=item I<_zeros(OBJ)>

Return the number of trailing decimal zeros. The output is a Perl scalar.

=item I<_digit(OBJ, N)>

Return the Nth digit as a Perl scalar. N is a Perl scalar, where zero refers to
the rightmost (least significant) digit, and negative values count from the
left (most significant digit). If $obj represents the number 123, then
I<$obj->_digit(0)> is 3 and I<_digit(123, -1)> is 1.

=item I<_check(OBJ)>

Return a true value if the object is OK, and a false value otherwise. This is a
check routine to test the internal state of the object for corruption.

=back

=head2 API version 2

The following methods are required for an API version of 2 or greater.

=head3 Constructors

=over 4

=item I<_1ex(N)>

Return an object representing the number 10**N where N E<gt>= 0 is a Perl
scalar.

=back

=head3 Mathematical functions

=over 4

=item I<_nok(OBJ1, OBJ2)>

Return the binomial coefficient OBJ1 over OBJ1.

=back

=head3 Miscellaneous

=over 4

=item I<_alen(OBJ)>

Return the approximate number of decimal digits of the object. The output is
one Perl scalar.

=back

=head2 API optional methods

The following methods are optional, and can be defined if the underlying lib
has a fast way to do them. If undefined, Math::BigInt will use pure Perl (hence
slow) fallback routines to emulate these:

=head3 Signed bitwise operators.

Each of these methods may modify the first input argument.

=over 4

=item I<_signed_or(OBJ1, OBJ2, SIGN1, SIGN2)>

Return the signed bitwise or.

=item I<_signed_and(OBJ1, OBJ2, SIGN1, SIGN2)>

Return the signed bitwise and.

=item I<_signed_xor(OBJ1, OBJ2, SIGN1, SIGN2)>

Return the signed bitwise exclusive or.

=back

=head1 WRAP YOUR OWN

If you want to port your own favourite c-lib for big numbers to the
Math::BigInt interface, you can take any of the already existing modules as a
rough guideline. You should really wrap up the latest Math::BigInt and
Math::BigFloat testsuites with your module, and replace in them any of the
following:

        use Math::BigInt;

by this:

        use Math::BigInt lib => 'yourlib';

This way you ensure that your library really works 100% within Math::BigInt.

=head1 BUGS

Please report any bugs or feature requests to
C<bug-math-bigint at rt.cpan.org>, or through the web interface at
L<https://rt.cpan.org/Ticket/Create.html?Queue=Math-BigInt>
(requires login).
We will be notified, and then you'll automatically be notified of progress on
your bug as I make changes.

=head1 SUPPORT

You can find documentation for this module with the perldoc command.

    perldoc Math::BigInt::Calc

You can also look for information at:

=over 4

=item * RT: CPAN's request tracker

L<https://rt.cpan.org/Public/Dist/Display.html?Name=Math-BigInt>

=item * AnnoCPAN: Annotated CPAN documentation

L<http://annocpan.org/dist/Math-BigInt>

=item * CPAN Ratings

L<http://cpanratings.perl.org/dist/Math-BigInt>

=item * Search CPAN

L<http://search.cpan.org/dist/Math-BigInt/>

=item * CPAN Testers Matrix

L<http://matrix.cpantesters.org/?dist=Math-BigInt>

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

=head1 AUTHORS

=over 4

=item *

Original math code by Mark Biggar, rewritten by Tels L<http://bloodgate.com/>
in late 2000.

=item *

Separated from BigInt and shaped API with the help of John Peacock.

=item *

Fixed, speed-up, streamlined and enhanced by Tels 2001 - 2007.

=item *

API documentation corrected and extended by Peter John Acklam,
E<lt>pjacklam@online.noE<gt>

=back

=head1 SEE ALSO

L<Math::BigInt>, L<Math::BigFloat>, L<Math::BigInt::GMP>,
L<Math::BigInt::FastCalc> and L<Math::BigInt::Pari>.

=cut
