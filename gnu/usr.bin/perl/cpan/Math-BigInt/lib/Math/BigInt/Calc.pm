package Math::BigInt::Calc;

use 5.006001;
use strict;
use warnings;

use Carp;
use Math::BigInt::Lib;

our $VERSION = '1.999811';

our @ISA = ('Math::BigInt::Lib');

# Package to store unsigned big integers in decimal and do math with them

# Internally the numbers are stored in an array with at least 1 element, no
# leading zero parts (except the first) and in base 1eX where X is determined
# automatically at loading time to be the maximum possible value

# todo:
# - fully remove funky $# stuff in div() (maybe - that code scares me...)

# USE_MUL: due to problems on certain os (os390, posix-bc) "* 1e-5" is used
# instead of "/ 1e5" at some places, (marked with USE_MUL). Other platforms
# BS2000, some Crays need USE_DIV instead.
# The BEGIN block is used to determine which of the two variants gives the
# correct result.

# Beware of things like:
# $i = $i * $y + $car; $car = int($i / $BASE); $i = $i % $BASE;
# This works on x86, but fails on ARM (SA1100, iPAQ) due to who knows what
# reasons. So, use this instead (slower, but correct):
# $i = $i * $y + $car; $car = int($i / $BASE); $i -= $BASE * $car;

##############################################################################
# global constants, flags and accessory

# announce that we are compatible with MBI v1.83 and up
sub api_version () { 2; }

# constants for easier life
my ($BASE, $BASE_LEN, $RBASE, $MAX_VAL);
my ($AND_BITS, $XOR_BITS, $OR_BITS);
my ($AND_MASK, $XOR_MASK, $OR_MASK);

sub _base_len {
    # Set/get the BASE_LEN and assorted other, related values.
    # Used only by the testsuite, the set variant is used only by the BEGIN
    # block below:

    my ($class, $b, $int) = @_;
    if (defined $b) {
        # avoid redefinitions
        undef &_mul;
        undef &_div;

        if ($] >= 5.008 && $int && $b > 7) {
            $BASE_LEN = $b;
            *_mul = \&_mul_use_div_64;
            *_div = \&_div_use_div_64;
            $BASE = int("1e" . $BASE_LEN);
            $MAX_VAL = $BASE-1;
            return $BASE_LEN unless wantarray;
            return ($BASE_LEN, $BASE, $AND_BITS, $XOR_BITS, $OR_BITS, $BASE_LEN, $MAX_VAL);
        }

        # find whether we can use mul or div in mul()/div()
        $BASE_LEN = $b + 1;
        my $caught = 0;
        while (--$BASE_LEN > 5) {
            $BASE = int("1e" . $BASE_LEN);
            $RBASE = abs('1e-' . $BASE_LEN); # see USE_MUL
            $caught = 0;
            $caught += 1 if (int($BASE * $RBASE) != 1); # should be 1
            $caught += 2 if (int($BASE / $BASE) != 1);  # should be 1
            last if $caught != 3;
        }
        $BASE = int("1e" . $BASE_LEN);
        $RBASE = abs('1e-' . $BASE_LEN); # see USE_MUL
        $MAX_VAL = $BASE-1;

        # ($caught & 1) != 0 => cannot use MUL
        # ($caught & 2) != 0 => cannot use DIV
        if ($caught == 2)       # 2
        {
            # must USE_MUL since we cannot use DIV
            *_mul = \&_mul_use_mul;
            *_div = \&_div_use_mul;
        } else                  # 0 or 1
        {
            # can USE_DIV instead
            *_mul = \&_mul_use_div;
            *_div = \&_div_use_div;
        }
    }
    return $BASE_LEN unless wantarray;
    return ($BASE_LEN, $BASE, $AND_BITS, $XOR_BITS, $OR_BITS, $BASE_LEN, $MAX_VAL);
}

sub _new {
    # Given a string representing an integer, returns a reference to an array
    # of integers, where each integer represents a chunk of the original input
    # integer.

    my ($class, $str) = @_;
    #unless ($str =~ /^([1-9]\d*|0)\z/) {
    #    require Carp;
    #    Carp::croak("Invalid input string '$str'");
    #}

    my $input_len = length($str) - 1;

    # Shortcut for small numbers.
    return bless [ $str ], $class if $input_len < $BASE_LEN;

    my $format = "a" . (($input_len % $BASE_LEN) + 1);
    $format .= $] < 5.008 ? "a$BASE_LEN" x int($input_len / $BASE_LEN)
                          : "(a$BASE_LEN)*";

    my $self = [ reverse(map { 0 + $_ } unpack($format, $str)) ];
    return bless $self, $class;
}

BEGIN {
    # from Daniel Pfeiffer: determine largest group of digits that is precisely
    # multipliable with itself plus carry
    # Test now changed to expect the proper pattern, not a result off by 1 or 2
    my ($e, $num) = 3;          # lowest value we will use is 3+1-1 = 3
    do {
        $num = '9' x ++$e;
        $num *= $num + 1;
    } while $num =~ /9{$e}0{$e}/; # must be a certain pattern
    $e--;                         # last test failed, so retract one step
    # the limits below brush the problems with the test above under the rug:
    # the test should be able to find the proper $e automatically
    $e = 5 if $^O =~ /^uts/;    # UTS get's some special treatment
    $e = 5 if $^O =~ /^unicos/; # unicos is also problematic (6 seems to work
                                # there, but we play safe)

    my $int = 0;
    if ($e > 7) {
        use integer;
        my $e1 = 7;
        $num = 7;
        do {
            $num = ('9' x ++$e1) + 0;
            $num *= $num + 1;
        } while ("$num" =~ /9{$e1}0{$e1}/); # must be a certain pattern
        $e1--;                  # last test failed, so retract one step
        if ($e1 > 7) {
            $int = 1;
            $e = $e1;
        }
    }

    __PACKAGE__ -> _base_len($e, $int);        # set and store

    use integer;
    # find out how many bits _and, _or and _xor can take (old default = 16)
    # I don't think anybody has yet 128 bit scalars, so let's play safe.
    local $^W = 0;              # don't warn about 'nonportable number'
    $AND_BITS = 15;
    $XOR_BITS = 15;
    $OR_BITS  = 15;

    # find max bits, we will not go higher than numberofbits that fit into $BASE
    # to make _and etc simpler (and faster for smaller, slower for large numbers)
    my $max = 16;
    while (2 ** $max < $BASE) {
        $max++;
    }
    {
        no integer;
        $max = 16 if $] < 5.006; # older Perls might not take >16 too well
    }
    my ($x, $y, $z);

    do {
        $AND_BITS++;
        $x = CORE::oct('0b' . '1' x $AND_BITS);
        $y = $x & $x;
        $z = (2 ** $AND_BITS) - 1;
    } while ($AND_BITS < $max && $x == $z && $y == $x);
    $AND_BITS --;               # retreat one step

    do {
        $XOR_BITS++;
        $x = CORE::oct('0b' . '1' x $XOR_BITS);
        $y = $x ^ 0;
        $z = (2 ** $XOR_BITS) - 1;
    } while ($XOR_BITS < $max && $x == $z && $y == $x);
    $XOR_BITS --;               # retreat one step

    do {
        $OR_BITS++;
        $x = CORE::oct('0b' . '1' x $OR_BITS);
        $y = $x | $x;
        $z = (2 ** $OR_BITS) -  1;
    } while ($OR_BITS < $max && $x == $z && $y == $x);
    $OR_BITS--;                # retreat one step

    $AND_MASK = __PACKAGE__->_new(( 2 ** $AND_BITS ));
    $XOR_MASK = __PACKAGE__->_new(( 2 ** $XOR_BITS ));
    $OR_MASK  = __PACKAGE__->_new(( 2 ** $OR_BITS  ));

    # We can compute the approximate length no faster than the real length:
    *_alen = \&_len;
}

###############################################################################

sub _zero {
    # create a zero
    my $class = shift;
    return bless [ 0 ], $class;
}

sub _one {
    # create a one
    my $class = shift;
    return bless [ 1 ], $class;
}

sub _two {
    # create a two
    my $class = shift;
    return bless [ 2 ], $class;
}

sub _ten {
    # create a 10
    my $class = shift;
    bless [ 10 ], $class;
}

sub _1ex {
    # create a 1Ex
    my $class = shift;

    my $rem   = $_[0] % $BASE_LEN;      # remainder
    my $parts = $_[0] / $BASE_LEN;      # parts

    # 000000, 000000, 100
    bless [ (0) x $parts, '1' . ('0' x $rem) ], $class;
}

sub _copy {
    # make a true copy
    my $class = shift;
    return bless [ @{ $_[0] } ], $class;
}

# catch and throw away
sub import { }

##############################################################################
# convert back to string and number

sub _str {
    # Convert number from internal base 1eN format to string format. Internal
    # format is always normalized, i.e., no leading zeros.

    my $ary = $_[1];
    my $idx = $#$ary;           # index of last element

    if ($idx < 0) {             # should not happen
        require Carp;
        Carp::croak("$_[1] has no elements");
    }

    # Handle first one differently, since it should not have any leading zeros.
    my $ret = int($ary->[$idx]);
    if ($idx > 0) {
        # Interestingly, the pre-padd method uses more time.
        # The old grep variant takes longer (14 vs. 10 sec).
        my $z = '0' x ($BASE_LEN - 1);
        while (--$idx >= 0) {
            $ret .= substr($z . $ary->[$idx], -$BASE_LEN);
        }
    }
    $ret;
}

sub _num {
    # Make a Perl scalar number (int/float) from a BigInt object.
    my $x = $_[1];

    return $x->[0] if @$x == 1;         # below $BASE

    # Start with the most significant element and work towards the least
    # significant element. Avoid multiplying "inf" (which happens if the number
    # overflows) with "0" (if there are zero elements in $x) since this gives
    # "nan" which propagates to the output.

    my $num = 0;
    for (my $i = $#$x ; $i >= 0 ; --$i) {
        $num *= $BASE;
        $num += $x -> [$i];
    }
    return $num;
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

sub _inc {
    # (ref to int_num_array, ref to int_num_array)
    # Add 1 to $x, modify $x in place
    my ($c, $x) = @_;

    for my $i (@$x) {
        return $x if ($i += 1) < $BASE; # early out
        $i = 0;                         # overflow, next
    }
    push @$x, 1 if $x->[-1] == 0;       # last overflowed, so extend
    $x;
}

sub _dec {
    # (ref to int_num_array, ref to int_num_array)
    # Sub 1 from $x, modify $x in place
    my ($c, $x) = @_;

    my $MAX = $BASE - 1;                # since MAX_VAL based on BASE
    for my $i (@$x) {
        last if ($i -= 1) >= 0;         # early out
        $i = $MAX;                      # underflow, next
    }
    pop @$x if $x->[-1] == 0 && @$x > 1; # last underflowed (but leave 0)
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

sub _mul_use_mul {
    # (ref to int_num_array, ref to int_num_array)
    # multiply two numbers in internal representation
    # modifies first arg, second need not be different from first
    my ($c, $xv, $yv) = @_;

    if (@$yv == 1) {
        # shortcut for two very short numbers (improved by Nathan Zook)
        # works also if xv and yv are the same reference, and handles also $x == 0
        if (@$xv == 1) {
            if (($xv->[0] *= $yv->[0]) >= $BASE) {
                $xv->[0] = $xv->[0] - ($xv->[1] = int($xv->[0] * $RBASE)) * $BASE;
            }
            ;
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
            $i = $i * $y + $car;
            $car = int($i * $RBASE);
            $i -= $car * $BASE;
        }
        push @$xv, $car if $car != 0;
        return $xv;
    }
    # shortcut for result $x == 0 => result = 0
    return $xv if @$xv == 1 && $xv->[0] == 0;

    # since multiplying $x with $x fails, make copy in this case
    $yv = [ @$xv ] if $xv == $yv; # same references?

    my @prod = ();
    my ($prod, $car, $cty, $xi, $yi);

    for $xi (@$xv) {
        $car = 0;
        $cty = 0;

        # slow variant
        #    for $yi (@$yv)
        #      {
        #      $prod = $xi * $yi + ($prod[$cty] || 0) + $car;
        #      $prod[$cty++] =
        #       $prod - ($car = int($prod * RBASE)) * $BASE;  # see USE_MUL
        #      }
        #    $prod[$cty] += $car if $car; # need really to check for 0?
        #    $xi = shift @prod;

        # faster variant
        # looping through this if $xi == 0 is silly - so optimize it away!
        $xi = (shift @prod || 0), next if $xi == 0;
        for $yi (@$yv) {
            $prod = $xi * $yi + ($prod[$cty] || 0) + $car;
            ##     this is actually a tad slower
            ##        $prod = $prod[$cty]; $prod += ($car + $xi * $yi);     # no ||0 here
            $prod[$cty++] =
              $prod - ($car = int($prod * $RBASE)) * $BASE; # see USE_MUL
        }
        $prod[$cty] += $car if $car; # need really to check for 0?
        $xi = shift @prod || 0;      # || 0 makes v5.005_3 happy
    }
    push @$xv, @prod;
    # can't have leading zeros
    #  __strip_zeros($xv);
    $xv;
}

sub _mul_use_div_64 {
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

sub _mul_use_div {
    # (ref to int_num_array, ref to int_num_array)
    # multiply two numbers in internal representation
    # modifies first arg, second need not be different from first
    my ($c, $xv, $yv) = @_;

    if (@$yv == 1) {
        # shortcut for two small numbers, also handles $x == 0
        if (@$xv == 1) {
            # shortcut for two very short numbers (improved by Nathan Zook)
            # works also if xv and yv are the same reference, and handles also $x == 0
            if (($xv->[0] *= $yv->[0]) >= $BASE) {
                $xv->[0] =
                  $xv->[0] - ($xv->[1] = int($xv->[0] / $BASE)) * $BASE;
            }
            ;
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
            $i = $i * $y + $car;
            $car = int($i / $BASE);
            $i -= $car * $BASE;
            # This (together with use integer;) does not work on 32-bit Perls
            #$i = $i * $y + $car; $i -= ($car = $i / $BASE) * $BASE;
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
            $prod[$cty++] = $prod - ($car = int($prod / $BASE)) * $BASE;
        }
        $prod[$cty] += $car if $car; # need really to check for 0?
        $xi = shift @prod || 0;      # || 0 makes v5.005_3 happy
    }
    push @$xv, @prod;
    # can't have leading zeros
    #  __strip_zeros($xv);
    $xv;
}

sub _div_use_mul {
    # ref to array, ref to array, modify first array and return remainder if
    # in list context

    # see comments in _div_use_div() for more explanations

    my ($c, $x, $yorg) = @_;

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

        # if $yorg has more digits than $x (it's leading element is longer than
        # the one from $x), the result will also be 0:
        if (length(int($yorg->[-1])) > length(int($x->[-1]))) {
            my $rem = $c->_copy($x) if wantarray;        # make copy
            @$x = 0;                            # set to 0
            return ($x, $rem) if wantarray;     # including remainder?
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
                # a = 0 => x == y => rem 0
                # a < 0 => x < y => rem = x
                my $rem = $a == 0 ? $c->_zero() : $c->_copy($x);
                @$x = 0;             # if $a < 0
                $x->[0] = 1 if $a == 0;  # $x == $y
                return ($x, $rem) if wantarray;
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
            $xi -= ($car = int($xi * $RBASE)) * $BASE; # see USE_MUL
        }
        push(@$x, $car);
        $car = 0;
        for $yi (@$y) {
            $yi = $yi * $dd + $car;
            $yi -= ($car = int($yi * $RBASE)) * $BASE; # see USE_MUL
        }
    } else {
        push(@$x, 0);
    }
    @q = ();
    ($v2, $v1) = @$y[-2, -1];
    $v2 = 0 unless $v2;
    while ($#$x > $#$y) {
        ($u2, $u1, $u0) = @$x[-3 .. -1];
        $u2 = 0 unless $u2;
        #warn "oups v1 is 0, u0: $u0 $y->[-2] $y->[-1] l ",scalar @$y,"\n"
        # if $v1 == 0;
        $q = (($u0 == $v1) ? $MAX_VAL : int(($u0 * $BASE + $u1) / $v1));
        --$q while ($v2 * $q > ($u0 * $BASE + $u1 - $q * $v1) * $BASE + $u2);
        if ($q) {
            ($car, $bar) = (0, 0);
            for ($yi = 0, $xi = $#$x - $#$y-1; $yi <= $#$y; ++$yi, ++$xi) {
                $prd = $q * $y->[$yi] + $car;
                $prd -= ($car = int($prd * $RBASE)) * $BASE; # see USE_MUL
                $x->[$xi] += $BASE if ($bar = (($x->[$xi] -= $prd + $bar) < 0));
            }
            if ($x->[-1] < $car + $bar) {
                $car = 0;
                --$q;
                for ($yi = 0, $xi = $#$x - $#$y-1; $yi <= $#$y; ++$yi, ++$xi) {
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
                $car = $prd - ($tmp = int($prd / $dd)) * $dd; # see USE_MUL
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

sub _div_use_div_64 {
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

sub _div_use_div {
    # ref to array, ref to array, modify first array and return remainder if
    # in list context
    my ($c, $x, $yorg) = @_;

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
            $rem = $c->_copy($x) if wantarray;        # make copy
            @$x = 0;                            # set to 0
            return ($x, $rem) if wantarray;     # including remainder?
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
                $rem = $c->_zero();                   # a = 0 => x == y => rem 0
                $rem = $c->_copy($x) if $a != 0;      # a < 0 => x < y => rem = x
                @$x = 0;
                $x->[0] = 0;                    # if $a < 0
                $x->[0] = 1 if $a == 0;         # $x == $y
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
        --$q while ($v2 * $q > ($u0 * $BASE + $u1 - $q * $v1) * $BASE + $u2);
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

##############################################################################
# testing

sub _acmp {
    # Internal absolute post-normalized compare (ignore signs)
    # ref to array, ref to array, return <0, 0, >0
    # Arrays must have at least one entry; this is not checked for.
    my ($c, $cx, $cy) = @_;

    # shortcut for short numbers
    return (($cx->[0] <=> $cy->[0]) <=> 0)
      if @$cx == 1 && @$cy == 1;

    # fast comp based on number of array elements (aka pseudo-length)
    my $lxy = (@$cx - @$cy)
      # or length of first element if same number of elements (aka difference 0)
      ||
        # need int() here because sometimes the last element is '00018' vs '18'
        (length(int($cx->[-1])) - length(int($cy->[-1])));

    return -1 if $lxy < 0;      # already differs, ret
    return  1 if $lxy > 0;      # ditto

    # manual way (abort if unequal, good for early ne)
    my $a;
    my $j = @$cx;
    while (--$j >= 0) {
        last if $a = $cx->[$j] - $cy->[$j];
    }
    $a <=> 0;
}

sub _len {
    # compute number of digits in base 10

    # int() because add/sub sometimes leaves strings (like '00005') instead of
    # '5' in this place, thus causing length() to report wrong length
    my $cx = $_[1];

    (@$cx - 1) * $BASE_LEN + length(int($cx->[-1]));
}

sub _digit {
    # Return the nth digit. Zero is rightmost, so _digit(123, 0) gives 3.
    # Negative values count from the left, so _digit(123, -1) gives 1.
    my ($c, $x, $n) = @_;

    my $len = _len('', $x);

    $n += $len if $n < 0;               # -1 last, -2 second-to-last

    # Math::BigInt::Calc returns 0 if N is out of range, but this is not done
    # by the other backend libraries.

    return "0" if $n < 0 || $n >= $len; # return 0 for digits out of range

    my $elem = int($n / $BASE_LEN);     # index of array element
    my $digit = $n % $BASE_LEN;         # index of digit within the element
    substr("0" x $BASE_LEN . "$x->[$elem]", -1 - $digit, 1);
}

sub _zeros {
    # Return number of trailing zeros in decimal.
    # Check each array element for having 0 at end as long as elem == 0
    # Upon finding a elem != 0, stop.

    my $x = $_[1];

    return 0 if @$x == 1 && $x->[0] == 0;

    my $zeros = 0;
    foreach my $elem (@$x) {
        if ($elem != 0) {
            $elem =~ /[^0](0*)\z/;
            $zeros += length($1);       # count trailing zeros
            last;                       # early out
        }
        $zeros += $BASE_LEN;
    }
    $zeros;
}

##############################################################################
# _is_* routines

sub _is_zero {
    # return true if arg is zero
    @{$_[1]} == 1 && $_[1]->[0] == 0 ? 1 : 0;
}

sub _is_even {
    # return true if arg is even
    $_[1]->[0] & 1 ? 0 : 1;
}

sub _is_odd {
    # return true if arg is odd
    $_[1]->[0] & 1 ? 1 : 0;
}

sub _is_one {
    # return true if arg is one
    @{$_[1]} == 1 && $_[1]->[0] == 1 ? 1 : 0;
}

sub _is_two {
    # return true if arg is two
    @{$_[1]} == 1 && $_[1]->[0] == 2 ? 1 : 0;
}

sub _is_ten {
    # return true if arg is ten
    @{$_[1]} == 1 && $_[1]->[0] == 10 ? 1 : 0;
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

    my $msg = $class -> SUPER::_check($x);
    return $msg if $msg;

    my $n;
    eval { $n = @$x };
    return "Not an array reference" unless $@ eq '';

    return "Reference to an empty array" unless $n > 0;

    # The following fails with Math::BigInt::FastCalc because a
    # Math::BigInt::FastCalc "object" is an unblessed array ref.
    #
    #return 0 unless ref($x) eq $class;

    for (my $i = 0 ; $i <= $#$x ; ++ $i) {
        my $e = $x -> [$i];

        return "Element at index $i is undefined"
          unless defined $e;

        return "Element at index $i is a '" . ref($e) .
          "', which is not a scalar"
          unless ref($e) eq "";

        # It would be better to use the regex /^([1-9]\d*|0)\z/, but that fails
        # in Math::BigInt::FastCalc, because it sometimes creates array
        # elements like "000000".
        return "Element at index $i is '$e', which does not look like an" .
          " normal integer" unless $e =~ /^\d+\z/;

        return "Element at index $i is '$e', which is not smaller than" .
          " the base '$BASE'" if $e >= $BASE;

        return "Element at index $i (last element) is zero"
          if $#$x > 0 && $i == $#$x && $e == 0;
    }

    return 0;
}

###############################################################################

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

##############################################################################
# shifts

sub _rsft {
    my ($c, $x, $y, $n) = @_;

    if ($n != 10) {
        $n = $c->_new($n);
        return scalar $c->_div($x, $c->_pow($n, $y));
    }

    # shortcut (faster) for shifting by 10)
    # multiples of $BASE_LEN
    my $dst = 0;                # destination
    my $src = $c->_num($y);     # as normal int
    my $xlen = (@$x - 1) * $BASE_LEN + length(int($x->[-1]));
    if ($src >= $xlen or ($src == $xlen and !defined $x->[1])) {
        # 12345 67890 shifted right by more than 10 digits => 0
        splice(@$x, 1);         # leave only one element
        $x->[0] = 0;            # set to zero
        return $x;
    }
    my $rem = $src % $BASE_LEN;   # remainder to shift
    $src = int($src / $BASE_LEN); # source
    if ($rem == 0) {
        splice(@$x, 0, $src);   # even faster, 38.4 => 39.3
    } else {
        my $len = @$x - $src;   # elems to go
        my $vd;
        my $z = '0' x $BASE_LEN;
        $x->[ @$x ] = 0;          # avoid || 0 test inside loop
        while ($dst < $len) {
            $vd = $z . $x->[$src];
            $vd = substr($vd, -$BASE_LEN, $BASE_LEN - $rem);
            $src++;
            $vd = substr($z . $x->[$src], -$rem, $rem) . $vd;
            $vd = substr($vd, -$BASE_LEN, $BASE_LEN) if length($vd) > $BASE_LEN;
            $x->[$dst] = int($vd);
            $dst++;
        }
        splice(@$x, $dst) if $dst > 0;       # kill left-over array elems
        pop @$x if $x->[-1] == 0 && @$x > 1; # kill last element if 0
    }                                        # else rem == 0
    $x;
}

sub _lsft {
    my ($c, $x, $n, $b) = @_;

    return $x if $c->_is_zero($x);

    # Handle the special case when the base is a power of 10. Don't check
    # whether log($b)/log(10) is an integer, because log(1000)/log(10) is not
    # exactly 3.

    my $log10 = sprintf "%.0f", log($b) / log(10);
    if ($b == 10 ** $log10) {
        $b = 10;
        $n = $c->_mul($n, $c->_new($log10));

        # shortcut (faster) for shifting by 10) since we are in base 10eX
        # multiples of $BASE_LEN:
        my $src = @$x;                      # source
        my $len = $c->_num($n);             # shift-len as normal int
        my $rem = $len % $BASE_LEN;         # remainder to shift
        my $dst = $src + int($len / $BASE_LEN); # destination
        my $vd;                                 # further speedup
        $x->[$src] = 0;                         # avoid first ||0 for speed
        my $z = '0' x $BASE_LEN;
        while ($src >= 0) {
            $vd = $x->[$src];
            $vd = $z . $vd;
            $vd = substr($vd, -$BASE_LEN + $rem, $BASE_LEN - $rem);
            $vd .= $src > 0 ? substr($z . $x->[$src - 1], -$BASE_LEN, $rem)
              : '0' x $rem;
            $vd = substr($vd, -$BASE_LEN, $BASE_LEN) if length($vd) > $BASE_LEN;
            $x->[$dst] = int($vd);
            $dst--;
            $src--;
        }
        # set lowest parts to 0
        while ($dst >= 0) {
            $x->[$dst--] = 0;
        }
        # fix spurious last zero element
        splice @$x, -1 if $x->[-1] == 0;
        return $x;
    } else {
        $b = $c->_new($b);
        #print $c->_str($b);
        return $c->_mul($x, $c->_pow($b, $n));
    }
}

sub _pow {
    # power of $x to $y
    # ref to array, ref to array, return ref to array
    my ($c, $cx, $cy) = @_;

    if (@$cy == 1 && $cy->[0] == 0) {
        splice(@$cx, 1);
        $cx->[0] = 1;           # y == 0 => x => 1
        return $cx;
    }

    if ((@$cx == 1 && $cx->[0] == 1) || #    x == 1
        (@$cy == 1 && $cy->[0] == 1))   # or y == 1
    {
        return $cx;
    }

    if (@$cx == 1 && $cx->[0] == 0) {
        splice (@$cx, 1);
        $cx->[0] = 0;           # 0 ** y => 0 (if not y <= 0)
        return $cx;
    }

    my $pow2 = $c->_one();

    my $y_bin = $c->_as_bin($cy);
    $y_bin =~ s/^0b//;
    my $len = length($y_bin);
    while (--$len > 0) {
        $c->_mul($pow2, $cx) if substr($y_bin, $len, 1) eq '1'; # is odd?
        $c->_mul($cx, $cx);
    }

    $c->_mul($cx, $pow2);
    $cx;
}

sub _nok {
    # Return binomial coefficient (n over k).
    # Given refs to arrays, return ref to array.
    # First input argument is modified.

    my ($c, $n, $k) = @_;

    # If k > n/2, or, equivalently, 2*k > n, compute nok(n, k) as
    # nok(n, n-k), to minimize the number if iterations in the loop.

    {
        my $twok = $c->_mul($c->_two(), $c->_copy($k)); # 2 * k
        if ($c->_acmp($twok, $n) > 0) {               # if 2*k > n
            $k = $c->_sub($c->_copy($n), $k);         # k = n - k
        }
    }

    # Example:
    #
    # / 7 \       7!       1*2*3*4 * 5*6*7   5 * 6 * 7       6   7
    # |   | = --------- =  --------------- = --------- = 5 * - * -
    # \ 3 /   (7-3)! 3!    1*2*3*4 * 1*2*3   1 * 2 * 3       2   3

    if ($c->_is_zero($k)) {
        @$n = 1;
    } else {

        # Make a copy of the original n, since we'll be modifying n in-place.

        my $n_orig = $c->_copy($n);

        # n = 5, f = 6, d = 2 (cf. example above)

        $c->_sub($n, $k);
        $c->_inc($n);

        my $f = $c->_copy($n);
        $c->_inc($f);

        my $d = $c->_two();

        # while f <= n (the original n, that is) ...

        while ($c->_acmp($f, $n_orig) <= 0) {

            # n = (n * f / d) == 5 * 6 / 2 (cf. example above)

            $c->_mul($n, $f);
            $c->_div($n, $d);

            # f = 7, d = 3 (cf. example above)

            $c->_inc($f);
            $c->_inc($d);
        }

    }

    return $n;
}

my @factorials = (
                  1,
                  1,
                  2,
                  2*3,
                  2*3*4,
                  2*3*4*5,
                  2*3*4*5*6,
                  2*3*4*5*6*7,
                 );

sub _fac {
    # factorial of $x
    # ref to array, return ref to array
    my ($c, $cx) = @_;

    if ((@$cx == 1) && ($cx->[0] <= 7)) {
        $cx->[0] = $factorials[$cx->[0]]; # 0 => 1, 1 => 1, 2 => 2 etc.
        return $cx;
    }

    if ((@$cx == 1) &&          # we do this only if $x >= 12 and $x <= 7000
        ($cx->[0] >= 12 && $cx->[0] < 7000)) {

        # Calculate (k-j) * (k-j+1) ... k .. (k+j-1) * (k + j)
        # See http://blogten.blogspot.com/2007/01/calculating-n.html
        # The above series can be expressed as factors:
        #   k * k - (j - i) * 2
        # We cache k*k, and calculate (j * j) as the sum of the first j odd integers

        # This will not work when N exceeds the storage of a Perl scalar, however,
        # in this case the algorithm would be way too slow to terminate, anyway.

        # As soon as the last element of $cx is 0, we split it up and remember
        # how many zeors we got so far. The reason is that n! will accumulate
        # zeros at the end rather fast.
        my $zero_elements = 0;

        # If n is even, set n = n -1
        my $k = $c->_num($cx);
        my $even = 1;
        if (($k & 1) == 0) {
            $even = $k;
            $k --;
        }
        # set k to the center point
        $k = ($k + 1) / 2;
        #  print "k $k even: $even\n";
        # now calculate k * k
        my $k2 = $k * $k;
        my $odd = 1;
        my $sum = 1;
        my $i = $k - 1;
        # keep reference to x
        my $new_x = $c->_new($k * $even);
        @$cx = @$new_x;
        if ($cx->[0] == 0) {
            $zero_elements ++;
            shift @$cx;
        }
        #  print STDERR "x = ", $c->_str($cx), "\n";
        my $BASE2 = int(sqrt($BASE))-1;
        my $j = 1;
        while ($j <= $i) {
            my $m = ($k2 - $sum);
            $odd += 2;
            $sum += $odd;
            $j++;
            while ($j <= $i && ($m < $BASE2) && (($k2 - $sum) < $BASE2)) {
                $m *= ($k2 - $sum);
                $odd += 2;
                $sum += $odd;
                $j++;
                #      print STDERR "\n k2 $k2 m $m sum $sum odd $odd\n"; sleep(1);
            }
            if ($m < $BASE) {
                $c->_mul($cx, [$m]);
            } else {
                $c->_mul($cx, $c->_new($m));
            }
            if ($cx->[0] == 0) {
                $zero_elements ++;
                shift @$cx;
            }
            #    print STDERR "Calculate $k2 - $sum = $m (x = ", $c->_str($cx), ")\n";
        }
        # multiply in the zeros again
        unshift @$cx, (0) x $zero_elements;
        return $cx;
    }

    # go forward until $base is exceeded limit is either $x steps (steps == 100
    # means a result always too high) or $base.
    my $steps = 100;
    $steps = $cx->[0] if @$cx == 1;
    my $r = 2;
    my $cf = 3;
    my $step = 2;
    my $last = $r;
    while ($r * $cf < $BASE && $step < $steps) {
        $last = $r;
        $r *= $cf++;
        $step++;
    }
    if ((@$cx == 1) && $step == $cx->[0]) {
        # completely done, so keep reference to $x and return
        $cx->[0] = $r;
        return $cx;
    }

    # now we must do the left over steps
    my $n;                      # steps still to do
    if (@$cx == 1) {
        $n = $cx->[0];
    } else {
        $n = $c->_copy($cx);
    }

    # Set $cx to the last result below $BASE (but keep ref to $x)
    $cx->[0] = $last;
    splice (@$cx, 1);
    # As soon as the last element of $cx is 0, we split it up and remember
    # how many zeors we got so far. The reason is that n! will accumulate
    # zeros at the end rather fast.
    my $zero_elements = 0;

    # do left-over steps fit into a scalar?
    if (ref $n eq 'ARRAY') {
        # No, so use slower inc() & cmp()
        # ($n is at least $BASE here)
        my $base_2 = int(sqrt($BASE)) - 1;
        #print STDERR "base_2: $base_2\n";
        while ($step < $base_2) {
            if ($cx->[0] == 0) {
                $zero_elements ++;
                shift @$cx;
            }
            my $b = $step * ($step + 1);
            $step += 2;
            $c->_mul($cx, [$b]);
        }
        $step = [$step];
        while ($c->_acmp($step, $n) <= 0) {
            if ($cx->[0] == 0) {
                $zero_elements ++;
                shift @$cx;
            }
            $c->_mul($cx, $step);
            $c->_inc($step);
        }
    } else {
        # Yes, so we can speed it up slightly

        #    print "# left over steps $n\n";

        my $base_4 = int(sqrt(sqrt($BASE))) - 2;
        #print STDERR "base_4: $base_4\n";
        my $n4 = $n - 4;
        while ($step < $n4 && $step < $base_4) {
            if ($cx->[0] == 0) {
                $zero_elements ++;
                shift @$cx;
            }
            my $b = $step * ($step + 1);
            $step += 2;
            $b *= $step * ($step + 1);
            $step += 2;
            $c->_mul($cx, [$b]);
        }
        my $base_2 = int(sqrt($BASE)) - 1;
        my $n2 = $n - 2;
        #print STDERR "base_2: $base_2\n";
        while ($step < $n2 && $step < $base_2) {
            if ($cx->[0] == 0) {
                $zero_elements ++;
                shift @$cx;
            }
            my $b = $step * ($step + 1);
            $step += 2;
            $c->_mul($cx, [$b]);
        }
        # do what's left over
        while ($step <= $n) {
            $c->_mul($cx, [$step]);
            $step++;
            if ($cx->[0] == 0) {
                $zero_elements ++;
                shift @$cx;
            }
        }
    }
    # multiply in the zeros again
    unshift @$cx, (0) x $zero_elements;
    $cx;                        # return result
}

sub _log_int {
    # calculate integer log of $x to base $base
    # ref to array, ref to array - return ref to array
    my ($c, $x, $base) = @_;

    # X == 0 => NaN
    return if @$x == 1 && $x->[0] == 0;

    # BASE 0 or 1 => NaN
    return if @$base == 1 && $base->[0] < 2;

    # X == 1 => 0 (is exact)
    if (@$x == 1 && $x->[0] == 1) {
        @$x = 0;
        return $x, 1;
    }

    my $cmp = $c->_acmp($x, $base);

    # X == BASE => 1 (is exact)
    if ($cmp == 0) {
        @$x = 1;
        return $x, 1;
    }

    # 1 < X < BASE => 0 (is truncated)
    if ($cmp < 0) {
        @$x = 0;
        return $x, 0;
    }

    my $x_org = $c->_copy($x);  # preserve x

    # Compute a guess for the result based on:
    # $guess = int ( length_in_base_10(X) / ( log(base) / log(10) ) )
    my $len = $c->_len($x_org);
    my $log = log($base->[-1]) / log(10);

    # for each additional element in $base, we add $BASE_LEN to the result,
    # based on the observation that log($BASE, 10) is BASE_LEN and
    # log(x*y) == log(x) + log(y):
    $log += (@$base - 1) * $BASE_LEN;

    # calculate now a guess based on the values obtained above:
    my $res = int($len / $log);

    @$x = $res;
    my $trial = $c->_pow($c->_copy($base), $x);
    my $acmp = $c->_acmp($trial, $x_org);

    # Did we get the exact result?

    return $x, 1 if $acmp == 0;

    # Too small?

    while ($acmp < 0) {
        $c->_mul($trial, $base);
        $c->_inc($x);
        $acmp = $c->_acmp($trial, $x_org);
    }

    # Too big?

    while ($acmp > 0) {
        $c->_div($trial, $base);
        $c->_dec($x);
        $acmp = $c->_acmp($trial, $x_org);
    }

    return $x, 1 if $acmp == 0;         # result is exact
    return $x, 0;                       # result is too small
}

# for debugging:
use constant DEBUG => 0;
my $steps = 0;
sub steps { $steps };

sub _sqrt {
    # square-root of $x in place
    # Compute a guess of the result (by rule of thumb), then improve it via
    # Newton's method.
    my ($c, $x) = @_;

    if (@$x == 1) {
        # fits into one Perl scalar, so result can be computed directly
        $x->[0] = int(sqrt($x->[0]));
        return $x;
    }
    my $y = $c->_copy($x);
    # hopefully _len/2 is < $BASE, the -1 is to always undershot the guess
    # since our guess will "grow"
    my $l = int(($c->_len($x)-1) / 2);

    my $lastelem = $x->[-1];    # for guess
    my $elems = @$x - 1;
    # not enough digits, but could have more?
    if ((length($lastelem) <= 3) && ($elems > 1)) {
        # right-align with zero pad
        my $len = length($lastelem) & 1;
        print "$lastelem => " if DEBUG;
        $lastelem .= substr($x->[-2] . '0' x $BASE_LEN, 0, $BASE_LEN);
        # former odd => make odd again, or former even to even again
        $lastelem = $lastelem / 10 if (length($lastelem) & 1) != $len;
        print "$lastelem\n" if DEBUG;
    }

    # construct $x (instead of $c->_lsft($x, $l, 10)
    my $r = $l % $BASE_LEN;     # 10000 00000 00000 00000 ($BASE_LEN=5)
    $l = int($l / $BASE_LEN);
    print "l =  $l " if DEBUG;

    splice @$x, $l;              # keep ref($x), but modify it

    # we make the first part of the guess not '1000...0' but int(sqrt($lastelem))
    # that gives us:
    # 14400 00000 => sqrt(14400) => guess first digits to be 120
    # 144000 000000 => sqrt(144000) => guess 379

    print "$lastelem (elems $elems) => " if DEBUG;
    $lastelem = $lastelem / 10 if ($elems & 1 == 1); # odd or even?
    my $g = sqrt($lastelem);
    $g =~ s/\.//;               # 2.345 => 2345
    $r -= 1 if $elems & 1 == 0; # 70 => 7

    # padd with zeros if result is too short
    $x->[$l--] = int(substr($g . '0' x $r, 0, $r+1));
    print "now ", $x->[-1] if DEBUG;
    print " would have been ", int('1' . '0' x $r), "\n" if DEBUG;

    # If @$x > 1, we could compute the second elem of the guess, too, to create
    # an even better guess. Not implemented yet. Does it improve performance?
    $x->[$l--] = 0 while ($l >= 0); # all other digits of guess are zero

    print "start x= ", $c->_str($x), "\n" if DEBUG;
    my $two = $c->_two();
    my $last = $c->_zero();
    my $lastlast = $c->_zero();
    $steps = 0 if DEBUG;
    while ($c->_acmp($last, $x) != 0 && $c->_acmp($lastlast, $x) != 0) {
        $steps++ if DEBUG;
        $lastlast = $c->_copy($last);
        $last = $c->_copy($x);
        $c->_add($x, $c->_div($c->_copy($y), $x));
        $c->_div($x, $two );
        print " x= ", $c->_str($x), "\n" if DEBUG;
    }
    print "\nsteps in sqrt: $steps, " if DEBUG;
    $c->_dec($x) if $c->_acmp($y, $c->_mul($c->_copy($x), $x)) < 0; # overshot?
    print " final ", $x->[-1], "\n" if DEBUG;
    $x;
}

sub _root {
    # Take n'th root of $x in place.

    my ($c, $x, $n) = @_;

    # Small numbers.

    if (@$x == 1 && @$n == 1) {
        # Result can be computed directly. Adjust initial result for numerical
        # errors, e.g., int(1000**(1/3)) is 2, not 3.
        my $y = int($x->[0] ** (1 / $n->[0]));
        my $yp1 = $y + 1;
        $y = $yp1 if $yp1 ** $n->[0] == $x->[0];
        $x->[0] = $y;
        return $x;
    }

    # If x <= n, the result is always (truncated to) 1.

    if ((@$x > 1 || $x -> [0] > 0) &&           # if x is non-zero ...
        $c -> _acmp($x, $n) <= 0)               # ... and x <= n
    {
        my $one = $x -> _one();
        @$x = @$one;
        return $x;
    }

    # If $n is a power of two, take sqrt($x) repeatedly, e.g., root($x, 4) =
    # sqrt(sqrt($x)), root($x, 8) = sqrt(sqrt(sqrt($x))).

    my $b = $c -> _as_bin($n);
    if ($b =~ /0b1(0+)$/) {
        my $count = length($1);       # 0b100 => len('00') => 2
        my $cnt = $count;             # counter for loop
        unshift @$x, 0;               # add one element, together with one
                                      #   more below in the loop this makes 2
        while ($cnt-- > 0) {
            # 'Inflate' $x by adding one element, basically computing
            # $x * $BASE * $BASE. This gives us more $BASE_LEN digits for
            # result since len(sqrt($X)) approx == len($x) / 2.
            unshift @$x, 0;
            # Calculate sqrt($x), $x is now one element to big, again. In the
            # next round we make that two, again.
            $c -> _sqrt($x);
        }

        # $x is now one element too big, so truncate result by removing it.
        shift @$x;

        return $x;
    }

    my $DEBUG = 0;

    # Now the general case. This works by finding an initial guess. If this
    # guess is incorrect, a relatively small delta is chosen. This delta is
    # used to find a lower and upper limit for the correct value. The delta is
    # doubled in each iteration. When a lower and upper limit is found,
    # bisection is applied to narrow down the region until we have the correct
    # value.

    # Split x into mantissa and exponent in base 10, so that
    #
    #   x = xm * 10^xe, where 0 < xm < 1 and xe is an integer

    my $x_str = $c -> _str($x);
    my $xm    = "." . $x_str;
    my $xe    = length($x_str);

    # From this we compute the base 10 logarithm of x
    #
    #   log_10(x) = log_10(xm) + log_10(xe^10)
    #             = log(xm)/log(10) + xe
    #
    # and then the base 10 logarithm of y, where y = x^(1/n)
    #
    #   log_10(y) = log_10(x)/n

    my $log10x = log($xm) / log(10) + $xe;
    my $log10y = $log10x / $c -> _num($n);

    # And from this we compute ym and ye, the mantissa and exponent (in
    # base 10) of y, where 1 < ym <= 10 and ye is an integer.

    my $ye = int $log10y;
    my $ym = 10 ** ($log10y - $ye);

    # Finally, we scale the mantissa and exponent to incraese the integer
    # part of ym, before building the string representing our guess of y.

    if ($DEBUG) {
        print "\n";
        print "xm     = $xm\n";
        print "xe     = $xe\n";
        print "log10x = $log10x\n";
        print "log10y = $log10y\n";
        print "ym     = $ym\n";
        print "ye     = $ye\n";
        print "\n";
    }

    my $d = $ye < 15 ? $ye : 15;
    $ym *= 10 ** $d;
    $ye -= $d;

    my $y_str = sprintf('%.0f', $ym) . "0" x $ye;
    my $y = $c -> _new($y_str);

    if ($DEBUG) {
        print "ym     = $ym\n";
        print "ye     = $ye\n";
        print "\n";
        print "y_str  = $y_str (initial guess)\n";
        print "\n";
    }

    # See if our guess y is correct.

    my $trial = $c -> _pow($c -> _copy($y), $n);
    my $acmp  = $c -> _acmp($trial, $x);

    if ($acmp == 0) {
        @$x = @$y;
        return $x;
    }

    # Find a lower and upper limit for the correct value of y. Start off with a
    # delta value that is approximately the size of the accuracy of the guess.

    my $lower;
    my $upper;

    my $delta = $c -> _new("1" . ("0" x $ye));
    my $two   = $c -> _two();

    if ($acmp < 0) {
        $lower = $y;
        while ($acmp < 0) {
            $upper = $c -> _add($c -> _copy($lower), $delta);

            if ($DEBUG) {
                print "lower  = $lower\n";
                print "upper  = $upper\n";
                print "delta  = $delta\n";
                print "\n";
            }
            $acmp  = $c -> _acmp($c -> _pow($c -> _copy($upper), $n), $x);
            if ($acmp == 0) {
                @$x = @$upper;
                return $x;
            }
            $delta = $c -> _mul($delta, $two);
        }
    }

    elsif ($acmp > 0) {
        $upper = $y;
        my $zero = $c -> _zero();
        while ($acmp > 0) {
            if ($c -> _acmp($upper, $delta) <= 0) {
                $lower = $c -> _zero();
                last;
            }
            $lower = $c -> _sub($c -> _copy($upper), $delta);

            if ($DEBUG) {
                print "lower  = $lower\n";
                print "upper  = $upper\n";
                print "delta  = $delta\n";
                print "\n";
            }
            $acmp  = $c -> _acmp($c -> _pow($c -> _copy($lower), $n), $x);
            if ($acmp == 0) {
                @$x = @$lower;
                return $x;
            }
            $delta = $c -> _mul($delta, $two);
        }
    }

    # Use bisection to narrow down the interval.

    my $one = $c -> _one();
    {

        $delta = $c -> _sub($c -> _copy($upper), $lower);
        if ($c -> _acmp($delta, $one) <= 0) {
            @$x = @$lower;
            return $x;
        }

        if ($DEBUG) {
            print "lower  = $lower\n";
            print "upper  = $upper\n";
            print "delta   = $delta\n";
            print "\n";
        }

        $delta = $c -> _div($delta, $two);
        my $middle = $c -> _add($c -> _copy($lower), $delta);

        $acmp  = $c -> _acmp($c -> _pow($c -> _copy($middle), $n), $x);
        if ($acmp < 0) {
            $lower = $middle;
        } elsif ($acmp > 0) {
            $upper = $middle;
        } else {
            @$x = @$middle;
            return $x;
        }

        redo;
    }

    $x;
}

##############################################################################
# binary stuff

sub _and {
    my ($c, $x, $y) = @_;

    # the shortcut makes equal, large numbers _really_ fast, and makes only a
    # very small performance drop for small numbers (e.g. something with less
    # than 32 bit) Since we optimize for large numbers, this is enabled.
    return $x if $c->_acmp($x, $y) == 0; # shortcut

    my $m = $c->_one();
    my ($xr, $yr);
    my $mask = $AND_MASK;

    my $x1 = $c->_copy($x);
    my $y1 = $c->_copy($y);
    my $z  = $c->_zero();

    use integer;
    until ($c->_is_zero($x1) || $c->_is_zero($y1)) {
        ($x1, $xr) = $c->_div($x1, $mask);
        ($y1, $yr) = $c->_div($y1, $mask);

        $c->_add($z, $c->_mul([ 0 + $xr->[0] & 0 + $yr->[0] ], $m));
        $c->_mul($m, $mask);
    }

    @$x = @$z;
    return $x;
}

sub _xor {
    my ($c, $x, $y) = @_;

    return $c->_zero() if $c->_acmp($x, $y) == 0; # shortcut (see -and)

    my $m = $c->_one();
    my ($xr, $yr);
    my $mask = $XOR_MASK;

    my $x1 = $c->_copy($x);
    my $y1 = $c->_copy($y);      # make copy
    my $z  = $c->_zero();

    use integer;
    until ($c->_is_zero($x1) || $c->_is_zero($y1)) {
        ($x1, $xr) = $c->_div($x1, $mask);
        ($y1, $yr) = $c->_div($y1, $mask);
        # make ints() from $xr, $yr (see _and())
        #$b = 1; $xrr = 0; foreach (@$xr) { $xrr += $_ * $b; $b *= $BASE; }
        #$b = 1; $yrr = 0; foreach (@$yr) { $yrr += $_ * $b; $b *= $BASE; }
        #$c->_add($x, $c->_mul($c->_new($xrr ^ $yrr)), $m) );

        $c->_add($z, $c->_mul([ 0 + $xr->[0] ^ 0 + $yr->[0] ], $m));
        $c->_mul($m, $mask);
    }
    # the loop stops when the shorter of the two numbers is exhausted
    # the remainder of the longer one will survive bit-by-bit, so we simple
    # multiply-add it in
    $c->_add($z, $c->_mul($x1, $m) ) if !$c->_is_zero($x1);
    $c->_add($z, $c->_mul($y1, $m) ) if !$c->_is_zero($y1);

    @$x = @$z;
    return $x;
}

sub _or {
    my ($c, $x, $y) = @_;

    return $x if $c->_acmp($x, $y) == 0; # shortcut (see _and)

    my $m = $c->_one();
    my ($xr, $yr);
    my $mask = $OR_MASK;

    my $x1 = $c->_copy($x);
    my $y1 = $c->_copy($y);      # make copy
    my $z  = $c->_zero();

    use integer;
    until ($c->_is_zero($x1) || $c->_is_zero($y1)) {
        ($x1, $xr) = $c->_div($x1, $mask);
        ($y1, $yr) = $c->_div($y1, $mask);
        # make ints() from $xr, $yr (see _and())
        #    $b = 1; $xrr = 0; foreach (@$xr) { $xrr += $_ * $b; $b *= $BASE; }
        #    $b = 1; $yrr = 0; foreach (@$yr) { $yrr += $_ * $b; $b *= $BASE; }
        #    $c->_add($x, $c->_mul(_new( $c, ($xrr | $yrr) ), $m) );

        $c->_add($z, $c->_mul([ 0 + $xr->[0] | 0 + $yr->[0] ], $m));
        $c->_mul($m, $mask);
    }
    # the loop stops when the shorter of the two numbers is exhausted
    # the remainder of the longer one will survive bit-by-bit, so we simple
    # multiply-add it in
    $c->_add($z, $c->_mul($x1, $m) ) if !$c->_is_zero($x1);
    $c->_add($z, $c->_mul($y1, $m) ) if !$c->_is_zero($y1);

    @$x = @$z;
    return $x;
}

sub _as_hex {
    # convert a decimal number to hex (ref to array, return ref to string)
    my ($c, $x) = @_;

    # fits into one element (handle also 0x0 case)
    return sprintf("0x%x", $x->[0]) if @$x == 1;

    my $x1 = $c->_copy($x);

    my $es = '';
    my ($xr, $h, $x10000);
    if ($] >= 5.006) {
        $x10000 = [ 0x10000 ];
        $h = 'h4';
    } else {
        $x10000 = [ 0x1000 ];
        $h = 'h3';
    }
    while (@$x1 != 1 || $x1->[0] != 0) # _is_zero()
    {
        ($x1, $xr) = $c->_div($x1, $x10000);
        $es .= unpack($h, pack('V', $xr->[0]));
    }
    $es = reverse $es;
    $es =~ s/^[0]+//;           # strip leading zeros
    '0x' . $es;                 # return result prepended with 0x
}

sub _as_bin {
    # convert a decimal number to bin (ref to array, return ref to string)
    my ($c, $x) = @_;

    # fits into one element (and Perl recent enough), handle also 0b0 case
    # handle zero case for older Perls
    if ($] <= 5.005 && @$x == 1 && $x->[0] == 0) {
        my $t = '0b0';
        return $t;
    }
    if (@$x == 1 && $] >= 5.006) {
        my $t = sprintf("0b%b", $x->[0]);
        return $t;
    }
    my $x1 = $c->_copy($x);

    my $es = '';
    my ($xr, $b, $x10000);
    if ($] >= 5.006) {
        $x10000 = [ 0x10000 ];
        $b = 'b16';
    } else {
        $x10000 = [ 0x1000 ];
        $b = 'b12';
    }
    while (!(@$x1 == 1 && $x1->[0] == 0)) # _is_zero()
    {
        ($x1, $xr) = $c->_div($x1, $x10000);
        $es .= unpack($b, pack('v', $xr->[0]));
    }
    $es = reverse $es;
    $es =~ s/^[0]+//;           # strip leading zeros
    '0b' . $es;                 # return result prepended with 0b
}

sub _as_oct {
    # convert a decimal number to octal (ref to array, return ref to string)
    my ($c, $x) = @_;

    # fits into one element (handle also 0 case)
    return sprintf("0%o", $x->[0]) if @$x == 1;

    my $x1 = $c->_copy($x);

    my $es = '';
    my $xr;
    my $x1000 = [ 0100000 ];
    while (@$x1 != 1 || $x1->[0] != 0) # _is_zero()
    {
        ($x1, $xr) = $c->_div($x1, $x1000);
        $es .= reverse sprintf("%05o", $xr->[0]);
    }
    $es = reverse $es;
    $es =~ s/^0+//;             # strip leading zeros
    '0' . $es;                  # return result prepended with 0
}

sub _from_oct {
    # convert a octal number to decimal (string, return ref to array)
    my ($c, $os) = @_;

    # for older Perls, play safe
    my $m = [ 0100000 ];
    my $d = 5;                  # 5 digits at a time

    my $mul = $c->_one();
    my $x = $c->_zero();

    my $len = int((length($os) - 1) / $d);      # $d digit parts, w/o the '0'
    my $val;
    my $i = -$d;
    while ($len >= 0) {
        $val = substr($os, $i, $d);             # get oct digits
        $val = CORE::oct($val);
        $i -= $d;
        $len --;
        my $adder = [ $val ];
        $c->_add($x, $c->_mul($adder, $mul)) if $val != 0;
        $c->_mul($mul, $m) if $len >= 0;        # skip last mul
    }
    $x;
}

sub _from_hex {
    # convert a hex number to decimal (string, return ref to array)
    my ($c, $hs) = @_;

    my $m = $c->_new(0x10000000); # 28 bit at a time (<32 bit!)
    my $d = 7;                    # 7 digits at a time
    my $mul = $c->_one();
    my $x = $c->_zero();

    my $len = int((length($hs) - 2) / $d); # $d digit parts, w/o the '0x'
    my $val;
    my $i = -$d;
    while ($len >= 0) {
        $val = substr($hs, $i, $d);     # get hex digits
        $val =~ s/^0x// if $len == 0; # for last part only because
        $val = CORE::hex($val);       # hex does not like wrong chars
        $i -= $d;
        $len --;
        my $adder = [ $val ];
        # if the resulting number was to big to fit into one element, create a
        # two-element version (bug found by Mark Lakata - Thanx!)
        if (CORE::length($val) > $BASE_LEN) {
            $adder = $c->_new($val);
        }
        $c->_add($x, $c->_mul($adder, $mul)) if $val != 0;
        $c->_mul($mul, $m) if $len >= 0; # skip last mul
    }
    $x;
}

sub _from_bin {
    # convert a hex number to decimal (string, return ref to array)
    my ($c, $bs) = @_;

    # instead of converting X (8) bit at a time, it is faster to "convert" the
    # number to hex, and then call _from_hex.

    my $hs = $bs;
    $hs =~ s/^[+-]?0b//;                                # remove sign and 0b
    my $l = length($hs);                                # bits
    $hs = '0' x (8 - ($l % 8)) . $hs if ($l % 8) != 0;  # padd left side w/ 0
    my $h = '0x' . unpack('H*', pack ('B*', $hs));      # repack as hex

    $c->_from_hex($h);
}

##############################################################################
# special modulus functions

sub _modinv {
    # modular multiplicative inverse
    my ($c, $x, $y) = @_;

    # modulo zero
    if ($c->_is_zero($y)) {
        return undef, undef;
    }

    # modulo one
    if ($c->_is_one($y)) {
        return $c->_zero(), '+';
    }

    my $u = $c->_zero();
    my $v = $c->_one();
    my $a = $c->_copy($y);
    my $b = $c->_copy($x);

    # Euclid's Algorithm for bgcd(), only that we calc bgcd() ($a) and the result
    # ($u) at the same time. See comments in BigInt for why this works.
    my $q;
    my $sign = 1;
    {
        ($a, $q, $b) = ($b, $c->_div($a, $b));          # step 1
        last if $c->_is_zero($b);

        my $t = $c->_add(                               # step 2:
                         $c->_mul($c->_copy($v), $q),   #  t =   v * q
                         $u);                           #      + u
        $u = $v;                                        #  u = v
        $v = $t;                                        #  v = t
        $sign = -$sign;
        redo;
    }

    # if the gcd is not 1, then return NaN
    return (undef, undef) unless $c->_is_one($a);

    ($v, $sign == 1 ? '+' : '-');
}

sub _modpow {
    # modulus of power ($x ** $y) % $z
    my ($c, $num, $exp, $mod) = @_;

    # a^b (mod 1) = 0 for all a and b
    if ($c->_is_one($mod)) {
        @$num = 0;
        return $num;
    }

    # 0^a (mod m) = 0 if m != 0, a != 0
    # 0^0 (mod m) = 1 if m != 0
    if ($c->_is_zero($num)) {
        if ($c->_is_zero($exp)) {
            @$num = 1;
        } else {
            @$num = 0;
        }
        return $num;
    }

    #  $num = $c->_mod($num, $mod);   # this does not make it faster

    my $acc = $c->_copy($num);
    my $t = $c->_one();

    my $expbin = $c->_as_bin($exp);
    $expbin =~ s/^0b//;
    my $len = length($expbin);
    while (--$len >= 0) {
        if (substr($expbin, $len, 1) eq '1') { # is_odd
            $t = $c->_mul($t, $acc);
            $t = $c->_mod($t, $mod);
        }
        $acc = $c->_mul($acc, $acc);
        $acc = $c->_mod($acc, $mod);
    }
    @$num = @$t;
    $num;
}

sub _gcd {
    # Greatest common divisor.

    my ($c, $x, $y) = @_;

    # gcd(0, 0) = 0
    # gcd(0, a) = a, if a != 0

    if (@$x == 1 && $x->[0] == 0) {
        if (@$y == 1 && $y->[0] == 0) {
            @$x = 0;
        } else {
            @$x = @$y;
        }
        return $x;
    }

    # Until $y is zero ...

    until (@$y == 1 && $y->[0] == 0) {

        # Compute remainder.

        $c->_mod($x, $y);

        # Swap $x and $y.

        my $tmp = $c->_copy($x);
        @$x = @$y;
        $y = $tmp;              # no deref here; that would modify input $y
    }

    return $x;
}

1;

=pod

=head1 NAME

Math::BigInt::Calc - Pure Perl module to support Math::BigInt

=head1 SYNOPSIS

    # to use it with Math::BigInt
    use Math::BigInt lib => 'Calc';

    # to use it with Math::BigFloat
    use Math::BigFloat lib => 'Calc';

    # to use it with Math::BigRat
    use Math::BigRat lib => 'Calc';

=head1 DESCRIPTION

Math::BigInt::Calc inherits from Math::BigInt::Lib.

In this library, the numbers are represented in base B = 10**N, where N is the
largest possible value that does not cause overflow in the intermediate
computations. The base B elements are stored in an array, with the least
significant element stored in array element zero. There are no leading zero
elements, except a single zero element when the number is zero.

For instance, if B = 10000, the number 1234567890 is represented internally
as [7890, 3456, 12].

=head1 SEE ALSO

L<Math::BigInt::Lib> for a description of the API.

Alternative libraries L<Math::BigInt::FastCalc>, L<Math::BigInt::GMP>, and
L<Math::BigInt::Pari>.

Some of the modules that use these libraries L<Math::BigInt>,
L<Math::BigFloat>, and L<Math::BigRat>.

=cut
