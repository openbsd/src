package Math::BigInt::Calc;

use 5.005;
use strict;
# use warnings;	# dont use warnings for older Perls

require Exporter;
use vars qw/@ISA $VERSION/;
@ISA = qw(Exporter);

$VERSION = '0.30';

# Package to store unsigned big integers in decimal and do math with them

# Internally the numbers are stored in an array with at least 1 element, no
# leading zero parts (except the first) and in base 1eX where X is determined
# automatically at loading time to be the maximum possible value

# todo:
# - fully remove funky $# stuff (maybe)

# USE_MUL: due to problems on certain os (os390, posix-bc) "* 1e-5" is used
# instead of "/ 1e5" at some places, (marked with USE_MUL). Other platforms
# BS2000, some Crays need USE_DIV instead.
# The BEGIN block is used to determine which of the two variants gives the
# correct result.

##############################################################################
# global constants, flags and accessory
 
# constants for easier life
my $nan = 'NaN';
my ($MBASE,$BASE,$RBASE,$BASE_LEN,$MAX_VAL,$BASE_LEN2,$BASE_LEN_SMALL);
my ($AND_BITS,$XOR_BITS,$OR_BITS);
my ($AND_MASK,$XOR_MASK,$OR_MASK);
my ($LEN_CONVERT);

sub _base_len 
  {
  # set/get the BASE_LEN and assorted other, connected values
  # used only be the testsuite, set is used only by the BEGIN block below
  shift;

  my $b = shift;
  if (defined $b)
    {
    # find whether we can use mul or div or none in mul()/div()
    # (in last case reduce BASE_LEN_SMALL)
    $BASE_LEN_SMALL = $b+1;
    my $caught = 0;
    while (--$BASE_LEN_SMALL > 5)
      {
      $MBASE = int("1e".$BASE_LEN_SMALL);
      $RBASE = abs('1e-'.$BASE_LEN_SMALL);		# see USE_MUL
      $caught = 0;
      $caught += 1 if (int($MBASE * $RBASE) != 1);	# should be 1
      $caught += 2 if (int($MBASE / $MBASE) != 1);	# should be 1
      last if $caught != 3;
      }
    # BASE_LEN is used for anything else than mul()/div()
    $BASE_LEN = $BASE_LEN_SMALL;
    $BASE_LEN = shift if (defined $_[0]);		# one more arg?
    $BASE = int("1e".$BASE_LEN);

    $BASE_LEN2 = int($BASE_LEN_SMALL / 2);		# for mul shortcut
    $MBASE = int("1e".$BASE_LEN_SMALL);
    $RBASE = abs('1e-'.$BASE_LEN_SMALL);		# see USE_MUL
    $MAX_VAL = $MBASE-1;
    $LEN_CONVERT = 0;
    $LEN_CONVERT = 1 if $BASE_LEN_SMALL != $BASE_LEN;

    #print "BASE_LEN: $BASE_LEN MAX_VAL: $MAX_VAL BASE: $BASE RBASE: $RBASE ";
    #print "BASE_LEN_SMALL: $BASE_LEN_SMALL MBASE: $MBASE\n";

    undef &_mul;
    undef &_div;

    if ($caught & 1 != 0)
      {
      # must USE_MUL
      *{_mul} = \&_mul_use_mul;
      *{_div} = \&_div_use_mul;
      }
    else		# $caught must be 2, since it can't be 1 nor 3
      {
      # can USE_DIV instead
      *{_mul} = \&_mul_use_div;
      *{_div} = \&_div_use_div;
      }
    }
  return $BASE_LEN unless wantarray;
  return ($BASE_LEN, $AND_BITS, $XOR_BITS, $OR_BITS, $BASE_LEN_SMALL, $MAX_VAL);
  }

BEGIN
  {
  # from Daniel Pfeiffer: determine largest group of digits that is precisely
  # multipliable with itself plus carry
  # Test now changed to expect the proper pattern, not a result off by 1 or 2
  my ($e, $num) = 3;	# lowest value we will use is 3+1-1 = 3
  do 
    {
    $num = ('9' x ++$e) + 0;
    $num *= $num + 1.0;
    } while ("$num" =~ /9{$e}0{$e}/);	# must be a certain pattern
  $e--; 				# last test failed, so retract one step
  # the limits below brush the problems with the test above under the rug:
  # the test should be able to find the proper $e automatically
  $e = 5 if $^O =~ /^uts/;	# UTS get's some special treatment
  $e = 5 if $^O =~ /^unicos/;	# unicos is also problematic (6 seems to work
				# there, but we play safe)
  $e = 5 if $] < 5.006;		# cap, for older Perls
  $e = 7 if $e > 7;		# cap, for VMS, OS/390 and other 64 bit systems
				# 8 fails inside random testsuite, so take 7

  # determine how many digits fit into an integer and can be safely added 
  # together plus carry w/o causing an overflow

  # this below detects 15 on a 64 bit system, because after that it becomes
  # 1e16  and not 1000000 :/ I can make it detect 18, but then I get a lot of
  # test failures. Ugh! (Tomake detect 18: uncomment lines marked with *)
  use integer;
  my $bi = 5;			# approx. 16 bit
  $num = int('9' x $bi);
  # $num = 99999; # *
  # while ( ($num+$num+1) eq '1' . '9' x $bi)	# *
  while ( int($num+$num+1) eq '1' . '9' x $bi)
    {
    $bi++; $num = int('9' x $bi);
    # $bi++; $num *= 10; $num += 9;	# *
    }
  $bi--;				# back off one step
  # by setting them equal, we ignore the findings and use the default
  # one-size-fits-all approach from former versions
  $bi = $e;				# XXX, this should work always

  __PACKAGE__->_base_len($e,$bi);	# set and store

  # find out how many bits _and, _or and _xor can take (old default = 16)
  # I don't think anybody has yet 128 bit scalars, so let's play safe.
  local $^W = 0;	# don't warn about 'nonportable number'
  $AND_BITS = 15; $XOR_BITS = 15; $OR_BITS  = 15;

  # find max bits, we will not go higher than numberofbits that fit into $BASE
  # to make _and etc simpler (and faster for smaller, slower for large numbers)
  my $max = 16;
  while (2 ** $max < $BASE) { $max++; }
  {
    no integer;
    $max = 16 if $] < 5.006;	# older Perls might not take >16 too well
  }
  my ($x,$y,$z);
  do {
    $AND_BITS++;
    $x = oct('0b' . '1' x $AND_BITS); $y = $x & $x;
    $z = (2 ** $AND_BITS) - 1;
    } while ($AND_BITS < $max && $x == $z && $y == $x);
  $AND_BITS --;						# retreat one step
  do {
    $XOR_BITS++;
    $x = oct('0b' . '1' x $XOR_BITS); $y = $x ^ 0;
    $z = (2 ** $XOR_BITS) - 1;
    } while ($XOR_BITS < $max && $x == $z && $y == $x);
  $XOR_BITS --;						# retreat one step
  do {
    $OR_BITS++;
    $x = oct('0b' . '1' x $OR_BITS); $y = $x | $x;
    $z = (2 ** $OR_BITS) - 1;
    } while ($OR_BITS < $max && $x == $z && $y == $x);
  $OR_BITS --;						# retreat one step
  
  }

##############################################################################
# convert between the "small" and the "large" representation

sub _to_large
  {
  # take an array in base $BASE_LEN_SMALL and convert it in-place to $BASE_LEN
  my ($c,$x) = @_;

#  print "_to_large $BASE_LEN_SMALL => $BASE_LEN\n";

  return $x if $LEN_CONVERT == 0 ||		# nothing to converconvertor
	 @$x == 1;				# only one element => early out
  
  #     12345    67890    12345    67890   contents
  # to      3        2        1        0   index 
  #             123456  7890123  4567890   contents 

#  # faster variant
#  my @d; my $str = '';
#  my $z = '0' x $BASE_LEN_SMALL;
#  foreach (@$x)
#    {
#    # ... . 04321 . 000321
#    $str = substr($z.$_,-$BASE_LEN_SMALL,$BASE_LEN_SMALL) . $str;
#    if (length($str) > $BASE_LEN)
#      {
#      push @d, substr($str,-$BASE_LEN,$BASE_LEN);	# extract one piece
#      substr($str,-$BASE_LEN,$BASE_LEN) = '';		# remove it
#      }
#    }
#  push @d, $str if $str !~ /^0*$/;			# extract last piece
#  @$x = @d;
#  $x->[-1] = int($x->[-1]);			# strip leading zero
#  $x;

  my $ret = "";
  my $l = scalar @$x;		# number of parts
  $l --; $ret .= int($x->[$l]); $l--;
  my $z = '0' x ($BASE_LEN_SMALL-1);                            
  while ($l >= 0)
    {
    $ret .= substr($z.$x->[$l],-$BASE_LEN_SMALL); 
    $l--;
    }
  my $str = _new($c,\$ret);			# make array
  @$x = @$str;					# clobber contents of $x
  $x->[-1] = int($x->[-1]);			# strip leading zero
  }

sub _to_small
  {
  # take an array in base $BASE_LEN and convert it in-place to $BASE_LEN_SMALL
  my ($c,$x) = @_;

  return $x if $LEN_CONVERT == 0;		# nothing to do
  return $x if @$x == 1 && length(int($x->[0])) <= $BASE_LEN_SMALL;

  my $d = _str($c,$x);
  my $il = length($$d)-1;
  ## this leaves '00000' instead of int 0 and will be corrected after any op
  # clobber contents of $x
  @$x = reverse(unpack("a" . ($il % $BASE_LEN_SMALL+1) 
      . ("a$BASE_LEN_SMALL" x ($il / $BASE_LEN_SMALL)), $$d));	

  $x->[-1] = int($x->[-1]);			# strip leading zero
  }

###############################################################################

sub _new
  {
  # (ref to string) return ref to num_array
  # Convert a number from string format (without sign) to internal base
  # 1ex format. Assumes normalized value as input.
  my $d = $_[1];
  my $il = length($$d)-1;
  # this leaves '00000' instead of int 0 and will be corrected after any op
  [ reverse(unpack("a" . ($il % $BASE_LEN+1) 
    . ("a$BASE_LEN" x ($il / $BASE_LEN)), $$d)) ];
  }                                                                             
  
BEGIN
  {
  $AND_MASK = __PACKAGE__->_new( \( 2 ** $AND_BITS ));
  $XOR_MASK = __PACKAGE__->_new( \( 2 ** $XOR_BITS ));
  $OR_MASK = __PACKAGE__->_new( \( 2 ** $OR_BITS ));
  }

sub _zero
  {
  # create a zero
  [ 0 ];
  }

sub _one
  {
  # create a one
  [ 1 ];
  }

sub _two
  {
  # create a two (used internally for shifting)
  [ 2 ];
  }

sub _copy
  {
  [ @{$_[1]} ];
  }

# catch and throw away
sub import { }

##############################################################################
# convert back to string and number

sub _str
  {
  # (ref to BINT) return num_str
  # Convert number from internal base 100000 format to string format.
  # internal format is always normalized (no leading zeros, "-0" => "+0")
  my $ar = $_[1];
  my $ret = "";

  my $l = scalar @$ar;		# number of parts
  return $nan if $l < 1;	# should not happen

  # handle first one different to strip leading zeros from it (there are no
  # leading zero parts in internal representation)
  $l --; $ret .= int($ar->[$l]); $l--;
  # Interestingly, the pre-padd method uses more time
  # the old grep variant takes longer (14 to 10 sec)
  my $z = '0' x ($BASE_LEN-1);                            
  while ($l >= 0)
    {
    $ret .= substr($z.$ar->[$l],-$BASE_LEN); # fastest way I could think of
    $l--;
    }
  \$ret;
  }                                                                             

sub _num
  {
  # Make a number (scalar int/float) from a BigInt object
  my $x = $_[1];
  return $x->[0] if scalar @$x == 1;  # below $BASE
  my $fac = 1;
  my $num = 0;
  foreach (@$x)
    {
    $num += $fac*$_; $fac *= $BASE;
    }
  $num; 
  }

##############################################################################
# actual math code

sub _add
  {
  # (ref to int_num_array, ref to int_num_array)
  # routine to add two base 1eX numbers
  # stolen from Knuth Vol 2 Algorithm A pg 231
  # there are separate routines to add and sub as per Knuth pg 233
  # This routine clobbers up array x, but not y.
 
  my ($c,$x,$y) = @_;

  return $x if (@$y == 1) && $y->[0] == 0;		# $x + 0 => $x
  if ((@$x == 1) && $x->[0] == 0)			# 0 + $y => $y->copy
    {
    # twice as slow as $x = [ @$y ], but necc. to retain $x as ref :(
    @$x = @$y; return $x;		
    }
 
  # for each in Y, add Y to X and carry. If after that, something is left in
  # X, foreach in X add carry to X and then return X, carry
  # Trades one "$j++" for having to shift arrays, $j could be made integer
  # but this would impose a limit to number-length of 2**32.
  my $i; my $car = 0; my $j = 0;
  for $i (@$y)
    {
    $x->[$j] -= $BASE if $car = (($x->[$j] += $i + $car) >= $BASE) ? 1 : 0;
    $j++;
    }
  while ($car != 0)
    {
    $x->[$j] -= $BASE if $car = (($x->[$j] += $car) >= $BASE) ? 1 : 0; $j++;
    }
  $x;
  }                                                                             

sub _inc
  {
  # (ref to int_num_array, ref to int_num_array)
  # routine to add 1 to a base 1eX numbers
  # This routine clobbers up array x, but not y.
  my ($c,$x) = @_;

  for my $i (@$x)
    {
    return $x if (($i += 1) < $BASE);		# early out
    $i = 0;					# overflow, next
    }
  push @$x,1 if ($x->[-1] == 0);		# last overflowed, so extend
  $x;
  }                                                                             

sub _dec
  {
  # (ref to int_num_array, ref to int_num_array)
  # routine to add 1 to a base 1eX numbers
  # This routine clobbers up array x, but not y.
  my ($c,$x) = @_;

  my $MAX = $BASE-1;				# since MAX_VAL based on MBASE
  for my $i (@$x)
    {
    last if (($i -= 1) >= 0);			# early out
    $i = $MAX;					# overflow, next
    }
  pop @$x if $x->[-1] == 0 && @$x > 1;		# last overflowed (but leave 0)
  $x;
  }                                                                             

sub _sub
  {
  # (ref to int_num_array, ref to int_num_array, swap)
  # subtract base 1eX numbers -- stolen from Knuth Vol 2 pg 232, $x > $y
  # subtract Y from X by modifying x in place
  my ($c,$sx,$sy,$s) = @_;
 
  my $car = 0; my $i; my $j = 0;
  if (!$s)
    {
    #print "case 2\n";
    for $i (@$sx)
      {
      last unless defined $sy->[$j] || $car;
      $i += $BASE if $car = (($i -= ($sy->[$j] || 0) + $car) < 0); $j++;
      }
    # might leave leading zeros, so fix that
    return __strip_zeros($sx);
    }
  #print "case 1 (swap)\n";
  for $i (@$sx)
    {
    # we can't do an early out if $x is < than $y, since we
    # need to copy the high chunks from $y. Found by Bob Mathews.
    #last unless defined $sy->[$j] || $car;
    $sy->[$j] += $BASE
     if $car = (($sy->[$j] = $i-($sy->[$j]||0) - $car) < 0);
    $j++;
    }
  # might leave leading zeros, so fix that
  __strip_zeros($sy);
  }                                                                             

sub _square_use_mul
  {
  # compute $x ** 2 or $x * $x in-place and return $x
  my ($c,$x) = @_;

  # From: Handbook of Applied Cryptography by A. Menezes, P. van Oorschot and
  #       S. Vanstone., Chapter 14

  #14.16 Algorithm Multiple-precision squaring
  #INPUT: positive integer x = (xt 1 xt 2 ... x1 x0)b.
  #OUTPUT: x * x = x ** 2 in radix b representation. 
  #1. For i from 0 to (2t - 1) do: wi <- 0. 
  #2.  For i from 0 to (t - 1) do the following: 
  # 2.1 (uv)b w2i + xi * xi, w2i v, c u. 
  # 2.2 For j from (i + 1)to (t - 1) do the following: 
  #      (uv)b <- wi+j + 2*xj * xi + c, wi+j <- v, c <- u. 
  # 2.3 wi+t <- u. 
  #3. Return((w2t-1 w2t-2 ... w1 w0)b).

#  # Note: That description is crap. Half of the symbols are not explained or
#  # used with out beeing set.
#  my $t = scalar @$x;		# count
#  my ($c,$i,$j);
#  for ($i = 0; $i < $t; $i++)
#    {
#    $x->[$i] = $x->[$i*2] + $x[$i]*$x[$i];
#    $x->[$i*2] = $x[$i]; $c = $x[$i];
#    for ($j = $i+1; $j < $t; $j++)
#      {
#      $x->[$i] = $x->[$i+$j] + 2 * $x->[$i] * $x->[$j];
#      $x->[$i+$j] = $x[$j]; $c = $x[$i];
#      }
#    $x->[$i+$t] = $x[$i];
#    }
  $x;
  }

sub _mul_use_mul
  {
  # (ref to int_num_array, ref to int_num_array)
  # multiply two numbers in internal representation
  # modifies first arg, second need not be different from first
  my ($c,$xv,$yv) = @_;

  # shortcut for two very short numbers (improved by Nathan Zook)
  # works also if xv and yv are the same reference
  if ((@$xv == 1) && (@$yv == 1))
    {
    if (($xv->[0] *= $yv->[0]) >= $MBASE)
       {
       $xv->[0] = $xv->[0] - ($xv->[1] = int($xv->[0] * $RBASE)) * $MBASE;
       };
    return $xv;
    }
  # shortcut for result == 0
  if ( ((@$xv == 1) && ($xv->[0] == 0)) ||
       ((@$yv == 1) && ($yv->[0] == 0)) )
    {
    @$xv = (0);
    return $xv;
    }

  # since multiplying $x with $x fails, make copy in this case
  $yv = [@$xv] if $xv == $yv;	# same references?
#  $yv = [@$xv] if "$xv" eq "$yv";	# same references?

  # since multiplying $x with $x would fail here, use the faster squaring
#  return _square($c,$xv) if $xv == $yv;	# same reference?

  if ($LEN_CONVERT != 0)
    {
    $c->_to_small($xv); $c->_to_small($yv);
    }

  my @prod = (); my ($prod,$car,$cty,$xi,$yi);

  for $xi (@$xv)
    {
    $car = 0; $cty = 0;

    # slow variant
#    for $yi (@$yv)
#      {
#      $prod = $xi * $yi + ($prod[$cty] || 0) + $car;
#      $prod[$cty++] =
#       $prod - ($car = int($prod * RBASE)) * $MBASE;  # see USE_MUL
#      }
#    $prod[$cty] += $car if $car; # need really to check for 0?
#    $xi = shift @prod;

    # faster variant
    # looping through this if $xi == 0 is silly - so optimize it away!
    $xi = (shift @prod || 0), next if $xi == 0;
    for $yi (@$yv)
      {
      $prod = $xi * $yi + ($prod[$cty] || 0) + $car;
##     this is actually a tad slower
##        $prod = $prod[$cty]; $prod += ($car + $xi * $yi);	# no ||0 here
      $prod[$cty++] =
       $prod - ($car = int($prod * $RBASE)) * $MBASE;  # see USE_MUL
      }
    $prod[$cty] += $car if $car; # need really to check for 0?
    $xi = shift @prod || 0;	# || 0 makes v5.005_3 happy
    }
  push @$xv, @prod;
  if ($LEN_CONVERT != 0)
    {
    $c->_to_large($yv);
    $c->_to_large($xv);
    }
  else
    {
    __strip_zeros($xv);
    }
  $xv;
  }                                                                             

sub _mul_use_div
  {
  # (ref to int_num_array, ref to int_num_array)
  # multiply two numbers in internal representation
  # modifies first arg, second need not be different from first
  my ($c,$xv,$yv) = @_;
 
  # shortcut for two very short numbers (improved by Nathan Zook)
  # works also if xv and yv are the same reference
  if ((@$xv == 1) && (@$yv == 1))
    {
    if (($xv->[0] *= $yv->[0]) >= $MBASE)
        {
        $xv->[0] =
            $xv->[0] - ($xv->[1] = int($xv->[0] / $MBASE)) * $MBASE;
        };
    return $xv;
    }
  # shortcut for result == 0
  if ( ((@$xv == 1) && ($xv->[0] == 0)) ||
       ((@$yv == 1) && ($yv->[0] == 0)) )
    {
    @$xv = (0);
    return $xv;
    }

 
  # since multiplying $x with $x fails, make copy in this case
  $yv = [@$xv] if $xv == $yv;	# same references?
#  $yv = [@$xv] if "$xv" eq "$yv";	# same references?
  # since multiplying $x with $x would fail here, use the faster squaring
#  return _square($c,$xv) if $xv == $yv;	# same reference?

  if ($LEN_CONVERT != 0)
    {
    $c->_to_small($xv); $c->_to_small($yv);
    }
  
  my @prod = (); my ($prod,$car,$cty,$xi,$yi);
  for $xi (@$xv)
    {
    $car = 0; $cty = 0;
    # looping through this if $xi == 0 is silly - so optimize it away!
    $xi = (shift @prod || 0), next if $xi == 0;
    for $yi (@$yv)
      {
      $prod = $xi * $yi + ($prod[$cty] || 0) + $car;
      $prod[$cty++] =
       $prod - ($car = int($prod / $MBASE)) * $MBASE;
      }
    $prod[$cty] += $car if $car; # need really to check for 0?
    $xi = shift @prod || 0;	# || 0 makes v5.005_3 happy
    }
  push @$xv, @prod;
  if ($LEN_CONVERT != 0)
    {
    $c->_to_large($yv);
    $c->_to_large($xv);
    }
  else
    {
    __strip_zeros($xv);
    }
  $xv;
  }                                                                             

sub _div_use_mul
  {
  # ref to array, ref to array, modify first array and return remainder if 
  # in list context
  my ($c,$x,$yorg) = @_;

  if (@$x == 1 && @$yorg == 1)
    {
    # shortcut, $yorg and $x are two small numbers
    if (wantarray)
      {
      my $r = [ $x->[0] % $yorg->[0] ];
      $x->[0] = int($x->[0] / $yorg->[0]);
      return ($x,$r); 
      }
    else
      {
      $x->[0] = int($x->[0] / $yorg->[0]);
      return $x; 
      }
    }
  if (@$yorg == 1)
    {
    my $rem;
    $rem = _mod($c,[ @$x ],$yorg) if wantarray;

    # shortcut, $y is < $BASE
    my $j = scalar @$x; my $r = 0; 
    my $y = $yorg->[0]; my $b;
    while ($j-- > 0)
      {
      $b = $r * $MBASE + $x->[$j];
      $x->[$j] = int($b/$y);
      $r = $b % $y;
      }
    pop @$x if @$x > 1 && $x->[-1] == 0;	# splice up a leading zero 
    return ($x,$rem) if wantarray;
    return $x;
    }

  my $y = [ @$yorg ];				# always make copy to preserve
  if ($LEN_CONVERT != 0)
    {
    $c->_to_small($x); $c->_to_small($y);
    }

  my ($car,$bar,$prd,$dd,$xi,$yi,@q,$v2,$v1,@d,$tmp,$q,$u2,$u1,$u0);

  $car = $bar = $prd = 0;
  if (($dd = int($MBASE/($y->[-1]+1))) != 1) 
    {
    for $xi (@$x) 
      {
      $xi = $xi * $dd + $car;
      $xi -= ($car = int($xi * $RBASE)) * $MBASE;	# see USE_MUL
      }
    push(@$x, $car); $car = 0;
    for $yi (@$y) 
      {
      $yi = $yi * $dd + $car;
      $yi -= ($car = int($yi * $RBASE)) * $MBASE;	# see USE_MUL
      }
    }
  else 
    {
    push(@$x, 0);
    }
  @q = (); ($v2,$v1) = @$y[-2,-1];
  $v2 = 0 unless $v2;
  while ($#$x > $#$y) 
    {
    ($u2,$u1,$u0) = @$x[-3..-1];
    $u2 = 0 unless $u2;
    #warn "oups v1 is 0, u0: $u0 $y->[-2] $y->[-1] l ",scalar @$y,"\n"
    # if $v1 == 0;
     $q = (($u0 == $v1) ? $MAX_VAL : int(($u0*$MBASE+$u1)/$v1));
    --$q while ($v2*$q > ($u0*$MBASE+$u1-$q*$v1)*$MBASE+$u2);
    if ($q)
      {
      ($car, $bar) = (0,0);
      for ($yi = 0, $xi = $#$x-$#$y-1; $yi <= $#$y; ++$yi,++$xi) 
        {
        $prd = $q * $y->[$yi] + $car;
        $prd -= ($car = int($prd * $RBASE)) * $MBASE;	# see USE_MUL
	$x->[$xi] += $MBASE if ($bar = (($x->[$xi] -= $prd + $bar) < 0));
	}
      if ($x->[-1] < $car + $bar) 
        {
        $car = 0; --$q;
	for ($yi = 0, $xi = $#$x-$#$y-1; $yi <= $#$y; ++$yi,++$xi) 
          {
	  $x->[$xi] -= $MBASE
	   if ($car = (($x->[$xi] += $y->[$yi] + $car) > $MBASE));
	  }
	}   
      }
      pop(@$x); unshift(@q, $q);
    }
  if (wantarray) 
    {
    @d = ();
    if ($dd != 1)  
      {
      $car = 0; 
      for $xi (reverse @$x) 
        {
        $prd = $car * $MBASE + $xi;
        $car = $prd - ($tmp = int($prd / $dd)) * $dd; # see USE_MUL
        unshift(@d, $tmp);
        }
      }
    else 
      {
      @d = @$x;
      }
    @$x = @q;
    my $d = \@d; 
    if ($LEN_CONVERT != 0)
      {
      $c->_to_large($x); $c->_to_large($d);
      }
    else
      {
      __strip_zeros($x);
      __strip_zeros($d);
      }
    return ($x,$d);
    }
  @$x = @q;
  if ($LEN_CONVERT != 0)
    {
    $c->_to_large($x);
    }
  else
    {
    __strip_zeros($x);
    }
  $x;
  }

sub _div_use_div
  {
  # ref to array, ref to array, modify first array and return remainder if 
  # in list context
  my ($c,$x,$yorg) = @_;

  if (@$x == 1 && @$yorg == 1)
    {
    # shortcut, $yorg and $x are two small numbers
    if (wantarray)
      {
      my $r = [ $x->[0] % $yorg->[0] ];
      $x->[0] = int($x->[0] / $yorg->[0]);
      return ($x,$r); 
      }
    else
      {
      $x->[0] = int($x->[0] / $yorg->[0]);
      return $x; 
      }
    }
  if (@$yorg == 1)
    {
    my $rem;
    $rem = _mod($c,[ @$x ],$yorg) if wantarray;

    # shortcut, $y is < $BASE
    my $j = scalar @$x; my $r = 0; 
    my $y = $yorg->[0]; my $b;
    while ($j-- > 0)
      {
      $b = $r * $MBASE + $x->[$j];
      $x->[$j] = int($b/$y);
      $r = $b % $y;
      }
    pop @$x if @$x > 1 && $x->[-1] == 0;	# splice up a leading zero 
    return ($x,$rem) if wantarray;
    return $x;
    }

  my $y = [ @$yorg ];				# always make copy to preserve
  if ($LEN_CONVERT != 0)
    {
    $c->_to_small($x); $c->_to_small($y);
    }
 
  my ($car,$bar,$prd,$dd,$xi,$yi,@q,$v2,$v1,@d,$tmp,$q,$u2,$u1,$u0);

  $car = $bar = $prd = 0;
  if (($dd = int($MBASE/($y->[-1]+1))) != 1) 
    {
    for $xi (@$x) 
      {
      $xi = $xi * $dd + $car;
      $xi -= ($car = int($xi / $MBASE)) * $MBASE;
      }
    push(@$x, $car); $car = 0;
    for $yi (@$y) 
      {
      $yi = $yi * $dd + $car;
      $yi -= ($car = int($yi / $MBASE)) * $MBASE;
      }
    }
  else 
    {
    push(@$x, 0);
    }
  @q = (); ($v2,$v1) = @$y[-2,-1];
  $v2 = 0 unless $v2;
  while ($#$x > $#$y) 
    {
    ($u2,$u1,$u0) = @$x[-3..-1];
    $u2 = 0 unless $u2;
    #warn "oups v1 is 0, u0: $u0 $y->[-2] $y->[-1] l ",scalar @$y,"\n"
    # if $v1 == 0;
     $q = (($u0 == $v1) ? $MAX_VAL : int(($u0*$MBASE+$u1)/$v1));
    --$q while ($v2*$q > ($u0*$MBASE+$u1-$q*$v1)*$MBASE+$u2);
    if ($q)
      {
      ($car, $bar) = (0,0);
      for ($yi = 0, $xi = $#$x-$#$y-1; $yi <= $#$y; ++$yi,++$xi) 
        {
        $prd = $q * $y->[$yi] + $car;
        $prd -= ($car = int($prd / $MBASE)) * $MBASE;
	$x->[$xi] += $MBASE if ($bar = (($x->[$xi] -= $prd + $bar) < 0));
	}
      if ($x->[-1] < $car + $bar) 
        {
        $car = 0; --$q;
	for ($yi = 0, $xi = $#$x-$#$y-1; $yi <= $#$y; ++$yi,++$xi) 
          {
	  $x->[$xi] -= $MBASE
	   if ($car = (($x->[$xi] += $y->[$yi] + $car) > $MBASE));
	  }
	}   
      }
    pop(@$x); unshift(@q, $q);
    }
  if (wantarray) 
    {
    @d = ();
    if ($dd != 1)  
      {
      $car = 0; 
      for $xi (reverse @$x) 
        {
        $prd = $car * $MBASE + $xi;
        $car = $prd - ($tmp = int($prd / $dd)) * $dd;
        unshift(@d, $tmp);
        }
      }
    else 
      {
      @d = @$x;
      }
    @$x = @q;
    my $d = \@d; 
    if ($LEN_CONVERT != 0)
      {
      $c->_to_large($x); $c->_to_large($d);
      }
    else
      {
      __strip_zeros($x);
      __strip_zeros($d);
      }
    return ($x,$d);
    }
  @$x = @q;
  if ($LEN_CONVERT != 0)
    {
    $c->_to_large($x);
    }
  else
    {
    __strip_zeros($x);
    }
  $x;
  }

##############################################################################
# testing

sub _acmp
  {
  # internal absolute post-normalized compare (ignore signs)
  # ref to array, ref to array, return <0, 0, >0
  # arrays must have at least one entry; this is not checked for

  my ($c,$cx,$cy) = @_;

  # fast comp based on number of array elements (aka pseudo-length)
  my $lxy = scalar @$cx - scalar @$cy;
  return -1 if $lxy < 0;				# already differs, ret
  return 1 if $lxy > 0;					# ditto
  
  # now calculate length based on digits, not parts
  $lxy = _len($c,$cx) - _len($c,$cy);			# difference
  return -1 if $lxy < 0;
  return 1 if $lxy > 0;

  # hm, same lengths,  but same contents?
  my $i = 0; my $a;
  # first way takes 5.49 sec instead of 4.87, but has the early out advantage
  # so grep is slightly faster, but more inflexible. hm. $_ instead of $k
  # yields 5.6 instead of 5.5 sec huh?
  # manual way (abort if unequal, good for early ne)
  my $j = scalar @$cx - 1;
  while ($j >= 0)
    {
    last if ($a = $cx->[$j] - $cy->[$j]); $j--;
    }
#  my $j = scalar @$cx;
#  while (--$j >= 0)
#    {
#    last if ($a = $cx->[$j] - $cy->[$j]);
#    }
  return 1 if $a > 0;
  return -1 if $a < 0;
  0;					# equal

  # while it early aborts, it is even slower than the manual variant
  #grep { return $a if ($a = $_ - $cy->[$i++]); } @$cx;
  # grep way, go trough all (bad for early ne)
  #grep { $a = $_ - $cy->[$i++]; } @$cx;
  #return $a;
  }

sub _len
  {
  # compute number of digits in bigint, minus the sign

  # int() because add/sub sometimes leaves strings (like '00005') instead of
  # '5' in this place, thus causing length() to report wrong length
  my $cx = $_[1];

  return (@$cx-1)*$BASE_LEN+length(int($cx->[-1]));
  }

sub _digit
  {
  # return the nth digit, negative values count backward
  # zero is rightmost, so _digit(123,0) will give 3
  my ($c,$x,$n) = @_;

  my $len = _len('',$x);

  $n = $len+$n if $n < 0;		# -1 last, -2 second-to-last
  $n = abs($n);				# if negative was too big
  $len--; $n = $len if $n > $len;	# n to big?
  
  my $elem = int($n / $BASE_LEN);	# which array element
  my $digit = $n % $BASE_LEN;		# which digit in this element
  $elem = '0000'.@$x[$elem];		# get element padded with 0's
  return substr($elem,-$digit-1,1);
  }

sub _zeros
  {
  # return amount of trailing zeros in decimal
  # check each array elem in _m for having 0 at end as long as elem == 0
  # Upon finding a elem != 0, stop
  my $x = $_[1];
  my $zeros = 0; my $elem;
  foreach my $e (@$x)
    {
    if ($e != 0)
      {
      $elem = "$e";				# preserve x
      $elem =~ s/.*?(0*$)/$1/;			# strip anything not zero
      $zeros *= $BASE_LEN;			# elems * 5
      $zeros += length($elem);			# count trailing zeros
      last;					# early out
      }
    $zeros ++;					# real else branch: 50% slower!
    }
  $zeros;
  }

##############################################################################
# _is_* routines

sub _is_zero
  {
  # return true if arg (BINT or num_str) is zero (array '+', '0')
  my $x = $_[1];

  (((scalar @$x == 1) && ($x->[0] == 0))) <=> 0;
  }

sub _is_even
  {
  # return true if arg (BINT or num_str) is even
  my $x = $_[1];
  (!($x->[0] & 1)) <=> 0; 
  }

sub _is_odd
  {
  # return true if arg (BINT or num_str) is even
  my $x = $_[1];

  (($x->[0] & 1)) <=> 0; 
  }

sub _is_one
  {
  # return true if arg (BINT or num_str) is one (array '+', '1')
  my $x = $_[1];

  (scalar @$x == 1) && ($x->[0] == 1) <=> 0; 
  }

sub __strip_zeros
  {
  # internal normalization function that strips leading zeros from the array
  # args: ref to array
  my $s = shift;
 
  my $cnt = scalar @$s; # get count of parts
  my $i = $cnt-1;
  push @$s,0 if $i < 0;		# div might return empty results, so fix it

  return $s if @$s == 1;		# early out

  #print "strip: cnt $cnt i $i\n";
  # '0', '3', '4', '0', '0',
  #  0    1    2    3    4
  # cnt = 5, i = 4
  # i = 4
  # i = 3
  # => fcnt = cnt - i (5-2 => 3, cnt => 5-1 = 4, throw away from 4th pos)
  # >= 1: skip first part (this can be zero)
  while ($i > 0) { last if $s->[$i] != 0; $i--; }
  $i++; splice @$s,$i if ($i < $cnt); # $i cant be 0
  $s;                                                                    
  }                                                                             

###############################################################################
# check routine to test internal state of corruptions

sub _check
  {
  # used by the test suite
  my $x = $_[1];

  return "$x is not a reference" if !ref($x);

  # are all parts are valid?
  my $i = 0; my $j = scalar @$x; my ($e,$try);
  while ($i < $j)
    {
    $e = $x->[$i]; $e = 'undef' unless defined $e;
    $try = '=~ /^[\+]?[0-9]+\$/; '."($x, $e)";
    last if $e !~ /^[+]?[0-9]+$/;
    $try = '=~ /^[\+]?[0-9]+\$/; '."($x, $e) (stringify)";
    last if "$e" !~ /^[+]?[0-9]+$/;
    $try = '=~ /^[\+]?[0-9]+\$/; '."($x, $e) (cat-stringify)";
    last if '' . "$e" !~ /^[+]?[0-9]+$/;
    $try = ' < 0 || >= $BASE; '."($x, $e)";
    last if $e <0 || $e >= $BASE;
    # this test is disabled, since new/bnorm and certain ops (like early out
    # in add/sub) are allowed/expected to leave '00000' in some elements
    #$try = '=~ /^00+/; '."($x, $e)";
    #last if $e =~ /^00+/;
    $i++;
    }
  return "Illegal part '$e' at pos $i (tested: $try)" if $i < $j;
  return 0;
  }


###############################################################################
###############################################################################
# some optional routines to make BigInt faster

sub _mod
  {
  # if possible, use mod shortcut
  my ($c,$x,$yo) = @_;

  # slow way since $y to big
  if (scalar @$yo > 1)
    {
    my ($xo,$rem) = _div($c,$x,$yo);
    return $rem;
    }
  my $y = $yo->[0];
  # both are single element arrays
  if (scalar @$x == 1)
    {
    $x->[0] %= $y;
    return $x;
    }

  # @y is single element, but @x has more than one
  my $b = $BASE % $y;
  if ($b == 0)
    {
    # when BASE % Y == 0 then (B * BASE) % Y == 0
    # (B * BASE) % $y + A % Y => A % Y
    # so need to consider only last element: O(1)
    $x->[0] %= $y;
    }
  elsif ($b == 1)
    {
    # else need to go trough all elements: O(N), but loop is a bit simplified
    my $r = 0;
    foreach (@$x)
      {
      $r = ($r + $_) % $y;		# not much faster, but heh...
      #$r += $_ % $y; $r %= $y;
      }
    $r = 0 if $r == $y;
    $x->[0] = $r;
    }
  else
    {
    # else need to go trough all elements: O(N)
    my $r = 0; my $bm = 1;
    foreach (@$x)
      {
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
  splice (@$x,1);
  $x;
  }

##############################################################################
# shifts

sub _rsft
  {
  my ($c,$x,$y,$n) = @_;

  if ($n != 10)
    {
    $n = _new($c,\$n); return _div($c,$x, _pow($c,$n,$y));
    }

  # shortcut (faster) for shifting by 10)
  # multiples of $BASE_LEN
  my $dst = 0;				# destination
  my $src = _num($c,$y);		# as normal int
  my $rem = $src % $BASE_LEN;		# remainder to shift
  $src = int($src / $BASE_LEN);		# source
  if ($rem == 0)
    {
    splice (@$x,0,$src);		# even faster, 38.4 => 39.3
    }
  else
    {
    my $len = scalar @$x - $src;	# elems to go
    my $vd; my $z = '0'x $BASE_LEN;
    $x->[scalar @$x] = 0;		# avoid || 0 test inside loop
    while ($dst < $len)
      {
      $vd = $z.$x->[$src];
      $vd = substr($vd,-$BASE_LEN,$BASE_LEN-$rem);
      $src++;
      $vd = substr($z.$x->[$src],-$rem,$rem) . $vd;
      $vd = substr($vd,-$BASE_LEN,$BASE_LEN) if length($vd) > $BASE_LEN;
      $x->[$dst] = int($vd);
      $dst++;
      }
    splice (@$x,$dst) if $dst > 0;		# kill left-over array elems
    pop @$x if $x->[-1] == 0 && @$x > 1;	# kill last element if 0
    } # else rem == 0
  $x;
  }

sub _lsft
  {
  my ($c,$x,$y,$n) = @_;

  if ($n != 10)
    {
    $n = _new($c,\$n); return _mul($c,$x, _pow($c,$n,$y));
    }

  # shortcut (faster) for shifting by 10) since we are in base 10eX
  # multiples of $BASE_LEN:
  my $src = scalar @$x;			# source
  my $len = _num($c,$y);		# shift-len as normal int
  my $rem = $len % $BASE_LEN;		# remainder to shift
  my $dst = $src + int($len/$BASE_LEN);	# destination
  my $vd;				# further speedup
  $x->[$src] = 0;			# avoid first ||0 for speed
  my $z = '0' x $BASE_LEN;
  while ($src >= 0)
    {
    $vd = $x->[$src]; $vd = $z.$vd;
    $vd = substr($vd,-$BASE_LEN+$rem,$BASE_LEN-$rem);
    $vd .= $src > 0 ? substr($z.$x->[$src-1],-$BASE_LEN,$rem) : '0' x $rem;
    $vd = substr($vd,-$BASE_LEN,$BASE_LEN) if length($vd) > $BASE_LEN;
    $x->[$dst] = int($vd);
    $dst--; $src--;
    }
  # set lowest parts to 0
  while ($dst >= 0) { $x->[$dst--] = 0; }
  # fix spurios last zero element
  splice @$x,-1 if $x->[-1] == 0;
  $x;
  }

sub _pow
  {
  # power of $x to $y
  # ref to array, ref to array, return ref to array
  my ($c,$cx,$cy) = @_;

  my $pow2 = _one();

  my $y_bin = ${_as_bin($c,$cy)}; $y_bin =~ s/^0b//;
  my $len = length($y_bin);
  while (--$len > 0)
    {
    _mul($c,$pow2,$cx) if substr($y_bin,$len,1) eq '1';		# is odd?
    _mul($c,$cx,$cx);
    }

  _mul($c,$cx,$pow2);
  $cx;
  }

sub _fac
  {
  # factorial of $x
  # ref to array, return ref to array
  my ($c,$cx) = @_;

  if ((@$cx == 1) && ($cx->[0] <= 2))
    {
    $cx->[0] = 1 * ($cx->[0]||1); # 0,1 => 1, 2 => 2
    return $cx;
    }

  # go forward until $base is exceeded
  # limit is either $x or $base (x == 100 means as result too high)
  my $steps = 100; $steps = $cx->[0] if @$cx == 1;
  my $r = 2; my $cf = 3; my $step = 1; my $last = $r;
  while ($r < $BASE && $step < $steps)
    {
    $last = $r; $r *= $cf++; $step++;
    }
  if ((@$cx == 1) && ($step == $cx->[0]))
    {
    # completely done
    $cx = [$last];
    return $cx;
    }
  my $n = _copy($c,$cx);
  $cx = [$last];

  #$cx = _one();
  while (!(@$n == 1 && $n->[0] == $step))
    {
    _mul($c,$cx,$n); _dec($c,$n);
    }
  $cx;
  }

use constant DEBUG => 0;

my $steps = 0;

sub steps { $steps };

sub _sqrt
  {
  # square-root of $x
  # ref to array, return ref to array
  my ($c,$x) = @_;

  if (scalar @$x == 1)
    {
    # fit's into one Perl scalar
    $x->[0] = int(sqrt($x->[0]));
    return $x;
    } 
  my $y = _copy($c,$x);
  # hopefully _len/2 is < $BASE, the -1 is to always undershot the guess
  # since our guess will "grow"
  my $l = int((_len($c,$x)-1) / 2);	

  my $lastelem = $x->[-1];	# for guess
  my $elems = scalar @$x - 1;
  # not enough digits, but could have more?
  if ((length($lastelem) <= 3) && ($elems > 1))	
    {
    # right-align with zero pad
    my $len = length($lastelem) & 1;
    print "$lastelem => " if DEBUG;
    $lastelem .= substr($x->[-2] . '0' x $BASE_LEN,0,$BASE_LEN);
    # former odd => make odd again, or former even to even again
    $lastelem = $lastelem / 10 if (length($lastelem) & 1) != $len;	
    print "$lastelem\n" if DEBUG;
    }

  # construct $x (instead of _lsft($c,$x,$l,10)
  my $r = $l % $BASE_LEN;	# 10000 00000 00000 00000 ($BASE_LEN=5)
  $l = int($l / $BASE_LEN);
  print "l =  $l " if DEBUG;
  
  splice @$x,$l; 		# keep ref($x), but modify it
 
  # we make the first part of the guess not '1000...0' but int(sqrt($lastelem))
  # that gives us:
  # 14400 00000 => sqrt(14400) => 120
  # 144000 000000 => sqrt(144000) => 379

  # $x->[$l--] = int('1' . '0' x $r);			# old way of guessing
  print "$lastelem (elems $elems) => " if DEBUG;
  $lastelem = $lastelem / 10 if ($elems & 1 == 1);		# odd or even?
  my $g = sqrt($lastelem); $g =~ s/\.//;			# 2.345 => 2345
  $r -= 1 if $elems & 1 == 0;					# 70 => 7

  # padd with zeros if result is too short
  $x->[$l--] = int(substr($g . '0' x $r,0,$r+1));
  print "now ",$x->[-1] if DEBUG;
  print " would have been ", int('1' . '0' x $r),"\n" if DEBUG;
  
  # If @$x > 1, we could compute the second elem of the guess, too, to create
  # an even better guess. Not implemented yet.
  $x->[$l--] = 0 while ($l >= 0);	# all other digits of guess are zero
 
  print "start x= ",${_str($c,$x)},"\n" if DEBUG;
  my $two = _two();
  my $last = _zero();
  my $lastlast = _zero();
  $steps = 0 if DEBUG;
  while (_acmp($c,$last,$x) != 0 && _acmp($c,$lastlast,$x) != 0)
    {
    $steps++ if DEBUG;
    $lastlast = _copy($c,$last);
    $last = _copy($c,$x);
    _add($c,$x, _div($c,_copy($c,$y),$x));
    _div($c,$x, $two );
    print "      x= ",${_str($c,$x)},"\n" if DEBUG;
    }
  print "\nsteps in sqrt: $steps, " if DEBUG;
  _dec($c,$x) if _acmp($c,$y,_mul($c,_copy($c,$x),$x)) < 0;	# overshot? 
  print " final ",$x->[-1],"\n" if DEBUG;
  $x;
  }

##############################################################################
# binary stuff

sub _and
  {
  my ($c,$x,$y) = @_;

  # the shortcut makes equal, large numbers _really_ fast, and makes only a
  # very small performance drop for small numbers (e.g. something with less
  # than 32 bit) Since we optimize for large numbers, this is enabled.
  return $x if _acmp($c,$x,$y) == 0;		# shortcut
  
  my $m = _one(); my ($xr,$yr);
  my $mask = $AND_MASK;

  my $x1 = $x;
  my $y1 = _copy($c,$y);			# make copy
  $x = _zero();
  my ($b,$xrr,$yrr);
  use integer;
  while (!_is_zero($c,$x1) && !_is_zero($c,$y1))
    {
    ($x1, $xr) = _div($c,$x1,$mask);
    ($y1, $yr) = _div($c,$y1,$mask);

    # make ints() from $xr, $yr
    # this is when the AND_BITS are greater tahn $BASE and is slower for
    # small (<256 bits) numbers, but faster for large numbers. Disabled
    # due to KISS principle

#    $b = 1; $xrr = 0; foreach (@$xr) { $xrr += $_ * $b; $b *= $BASE; }
#    $b = 1; $yrr = 0; foreach (@$yr) { $yrr += $_ * $b; $b *= $BASE; }
#    _add($c,$x, _mul($c, _new( $c, \($xrr & $yrr) ), $m) );
    
    # 0+ due to '&' doesn't work in strings
    _add($c,$x, _mul($c, [ 0+$xr->[0] & 0+$yr->[0] ], $m) );
    _mul($c,$m,$mask);
    }
  $x;
  }

sub _xor
  {
  my ($c,$x,$y) = @_;

  return _zero() if _acmp($c,$x,$y) == 0;	# shortcut (see -and)

  my $m = _one(); my ($xr,$yr);
  my $mask = $XOR_MASK;

  my $x1 = $x;
  my $y1 = _copy($c,$y);			# make copy
  $x = _zero();
  my ($b,$xrr,$yrr);
  use integer;
  while (!_is_zero($c,$x1) && !_is_zero($c,$y1))
    {
    ($x1, $xr) = _div($c,$x1,$mask);
    ($y1, $yr) = _div($c,$y1,$mask);
    # make ints() from $xr, $yr (see _and())
    #$b = 1; $xrr = 0; foreach (@$xr) { $xrr += $_ * $b; $b *= $BASE; }
    #$b = 1; $yrr = 0; foreach (@$yr) { $yrr += $_ * $b; $b *= $BASE; }
    #_add($c,$x, _mul($c, _new( $c, \($xrr ^ $yrr) ), $m) );

    # 0+ due to '^' doesn't work in strings
    _add($c,$x, _mul($c, [ 0+$xr->[0] ^ 0+$yr->[0] ], $m) );
    _mul($c,$m,$mask);
    }
  # the loop stops when the shorter of the two numbers is exhausted
  # the remainder of the longer one will survive bit-by-bit, so we simple
  # multiply-add it in
  _add($c,$x, _mul($c, $x1, $m) ) if !_is_zero($c,$x1);
  _add($c,$x, _mul($c, $y1, $m) ) if !_is_zero($c,$y1);
  
  $x;
  }

sub _or
  {
  my ($c,$x,$y) = @_;

  return $x if _acmp($c,$x,$y) == 0;		# shortcut (see _and)

  my $m = _one(); my ($xr,$yr);
  my $mask = $OR_MASK;

  my $x1 = $x;
  my $y1 = _copy($c,$y);			# make copy
  $x = _zero();
  my ($b,$xrr,$yrr);
  use integer;
  while (!_is_zero($c,$x1) && !_is_zero($c,$y1))
    {
    ($x1, $xr) = _div($c,$x1,$mask);
    ($y1, $yr) = _div($c,$y1,$mask);
    # make ints() from $xr, $yr (see _and())
#    $b = 1; $xrr = 0; foreach (@$xr) { $xrr += $_ * $b; $b *= $BASE; }
#    $b = 1; $yrr = 0; foreach (@$yr) { $yrr += $_ * $b; $b *= $BASE; }
#    _add($c,$x, _mul($c, _new( $c, \($xrr | $yrr) ), $m) );
    
    # 0+ due to '|' doesn't work in strings
    _add($c,$x, _mul($c, [ 0+$xr->[0] | 0+$yr->[0] ], $m) );
    _mul($c,$m,$mask);
    }
  # the loop stops when the shorter of the two numbers is exhausted
  # the remainder of the longer one will survive bit-by-bit, so we simple
  # multiply-add it in
  _add($c,$x, _mul($c, $x1, $m) ) if !_is_zero($c,$x1);
  _add($c,$x, _mul($c, $y1, $m) ) if !_is_zero($c,$y1);
  
  $x;
  }

sub _as_hex
  {
  # convert a decimal number to hex (ref to array, return ref to string)
  my ($c,$x) = @_;

  my $x1 = _copy($c,$x);

  my $es = '';
  my ($xr, $h, $x10000);
  if ($] >= 5.006)
    {
    $x10000 = [ 0x10000 ]; $h = 'h4';
    }
  else
    {
    $x10000 = [ 0x1000 ]; $h = 'h3';
    }
  while (! _is_zero($c,$x1))
    {
    ($x1, $xr) = _div($c,$x1,$x10000);
    $es .= unpack($h,pack('v',$xr->[0]));
    }
  $es = reverse $es;
  $es =~ s/^[0]+//;   # strip leading zeros
  $es = '0x' . $es;
  \$es;
  }

sub _as_bin
  {
  # convert a decimal number to bin (ref to array, return ref to string)
  my ($c,$x) = @_;

  my $x1 = _copy($c,$x);

  my $es = '';
  my ($xr, $b, $x10000);
  if ($] >= 5.006)
    {
    $x10000 = [ 0x10000 ]; $b = 'b16';
    }
  else
    {
    $x10000 = [ 0x1000 ]; $b = 'b12';
    }
  while (! _is_zero($c,$x1))
    {
    ($x1, $xr) = _div($c,$x1,$x10000);
    $es .= unpack($b,pack('v',$xr->[0]));
    }
  $es = reverse $es;
  $es =~ s/^[0]+//;   # strip leading zeros
  $es = '0b' . $es;
  \$es;
  }

sub _from_hex
  {
  # convert a hex number to decimal (ref to string, return ref to array)
  my ($c,$hs) = @_;

  my $mul = _one();
  my $m = [ 0x10000 ];				# 16 bit at a time
  my $x = _zero();

  my $len = length($$hs)-2;
  $len = int($len/4);				# 4-digit parts, w/o '0x'
  my $val; my $i = -4;
  while ($len >= 0)
    {
    $val = substr($$hs,$i,4);
    $val =~ s/^[+-]?0x// if $len == 0;		# for last part only because
    $val = hex($val);				# hex does not like wrong chars
    $i -= 4; $len --;
    _add ($c, $x, _mul ($c, [ $val ], $mul ) ) if $val != 0;
    _mul ($c, $mul, $m ) if $len >= 0; 		# skip last mul
    }
  $x;
  }

sub _from_bin
  {
  # convert a hex number to decimal (ref to string, return ref to array)
  my ($c,$bs) = @_;

  # instead of converting 8 bit at a time, it is faster to convert the
  # number to hex, and then call _from_hex.

  my $hs = $$bs;
  $hs =~ s/^[+-]?0b//;					# remove sign and 0b
  my $l = length($hs);					# bits
  $hs = '0' x (8-($l % 8)) . $hs if ($l % 8) != 0;	# padd left side w/ 0
  my $h = unpack('H*', pack ('B*', $hs));		# repack as hex
  return $c->_from_hex(\('0x'.$h));
 
  my $mul = _one();
  my $m = [ 0x100 ];				# 8 bit at a time
  my $x = _zero();

  my $len = length($$bs)-2;
  $len = int($len/8);				# 4-digit parts, w/o '0x'
  my $val; my $i = -8;
  while ($len >= 0)
    {
    $val = substr($$bs,$i,8);
    $val =~ s/^[+-]?0b// if $len == 0;		# for last part only

    $val = ord(pack('B8',substr('00000000'.$val,-8,8))); 

    $i -= 8; $len --;
    _add ($c, $x, _mul ($c, [ $val ], $mul ) ) if $val != 0;
    _mul ($c, $mul, $m ) if $len >= 0; 		# skip last mul
    }
  $x;
  }

##############################################################################
# special modulus functions

# not ready yet, since it would need to deal with unsigned numbers
sub _modinv1
  {
  # inverse modulus
  my ($c,$num,$mod) = @_;

  my $u = _zero(); my $u1 = _one();
  my $a = _copy($c,$mod); my $b = _copy($c,$num);

  # Euclid's Algorithm for bgcd(), only that we calc bgcd() ($a) and the
  # result ($u) at the same time
  while (!_is_zero($c,$b))
    {
#    print ${_str($c,$a)}, " ", ${_str($c,$b)}, " ", ${_str($c,$u)}, " ",
#     ${_str($c,$u1)}, "\n";
    ($a, my $q, $b) = ($b, _div($c,$a,$b));
#    print ${_str($c,$a)}, " ", ${_str($c,$q)}, " ", ${_str($c,$b)}, "\n";
    # original: ($u,$u1) = ($u1, $u - $u1 * $q);
    my $t = _copy($c,$u);
    $u = _copy($c,$u1);
    _mul($c,$u1,$q);
    $u1 = _sub($t,$u1);
#    print ${_str($c,$a)}, " ", ${_str($c,$b)}, " ", ${_str($c,$u)}, " ",
#     ${_str($c,$u1)}, "\n";
    }

  # if the gcd is not 1, then return NaN
  return undef unless _is_one($c,$a);

  $num = _mod($c,$u,$mod);
#  print ${_str($c,$num)},"\n";
  $num;
  }

sub _modpow
  {
  # modulus of power ($x ** $y) % $z
  my ($c,$num,$exp,$mod) = @_;

  # in the trivial case,
  if (_is_one($c,$mod))
    {
    splice @$num,0,1; $num->[0] = 0;
    return $num;
    }
  if ((scalar @$num == 1) && (($num->[0] == 0) || ($num->[0] == 1)))
    {
    $num->[0] = 1;
    return $num;
    }

#  $num = _mod($c,$num,$mod);	# this does not make it faster

  my $acc = _copy($c,$num); my $t = _one();

  my $expbin = ${_as_bin($c,$exp)}; $expbin =~ s/^0b//;
  my $len = length($expbin);
  while (--$len >= 0)
    {
    if ( substr($expbin,$len,1) eq '1')			# is_odd
      {
      _mul($c,$t,$acc);
      $t = _mod($c,$t,$mod);
      }
    _mul($c,$acc,$acc);
    $acc = _mod($c,$acc,$mod);
    }
  @$num = @$t;
  $num;
  }

##############################################################################
##############################################################################

1;
__END__

=head1 NAME

Math::BigInt::Calc - Pure Perl module to support Math::BigInt

=head1 SYNOPSIS

Provides support for big integer calculations. Not intended to be used by other
modules (except Math::BigInt::Cached). Other modules which sport the same
functions can also be used to support Math::Bigint, like Math::BigInt::Pari.

=head1 DESCRIPTION

In order to allow for multiple big integer libraries, Math::BigInt was
rewritten to use library modules for core math routines. Any module which
follows the same API as this can be used instead by using the following:

	use Math::BigInt lib => 'libname';

'libname' is either the long name ('Math::BigInt::Pari'), or only the short
version like 'Pari'.

=head1 EXPORT

The following functions MUST be defined in order to support the use by
Math::BigInt:

	_new(string)	return ref to new object from ref to decimal string
	_zero()		return a new object with value 0
	_one()		return a new object with value 1

	_str(obj)	return ref to a string representing the object
	_num(obj)	returns a Perl integer/floating point number
			NOTE: because of Perl numeric notation defaults,
			the _num'ified obj may lose accuracy due to 
			machine-dependend floating point size limitations
                    
	_add(obj,obj)	Simple addition of two objects
	_mul(obj,obj)	Multiplication of two objects
	_div(obj,obj)	Division of the 1st object by the 2nd
			In list context, returns (result,remainder).
			NOTE: this is integer math, so no
			fractional part will be returned.
	_sub(obj,obj)	Simple subtraction of 1 object from another
			a third, optional parameter indicates that the params
			are swapped. In this case, the first param needs to
			be preserved, while you can destroy the second.
			sub (x,y,1) => return x - y and keep x intact!
	_dec(obj)	decrement object by one (input is garant. to be > 0)
	_inc(obj)	increment object by one


	_acmp(obj,obj)	<=> operator for objects (return -1, 0 or 1)

	_len(obj)	returns count of the decimal digits of the object
	_digit(obj,n)	returns the n'th decimal digit of object

	_is_one(obj)	return true if argument is +1
	_is_zero(obj)	return true if argument is 0
	_is_even(obj)	return true if argument is even (0,2,4,6..)
	_is_odd(obj)	return true if argument is odd (1,3,5,7..)

	_copy		return a ref to a true copy of the object

	_check(obj)	check whether internal representation is still intact
			return 0 for ok, otherwise error message as string

The following functions are optional, and can be defined if the underlying lib
has a fast way to do them. If undefined, Math::BigInt will use pure Perl (hence
slow) fallback routines to emulate these:

	_from_hex(str)	return ref to new object from ref to hexadecimal string
	_from_bin(str)	return ref to new object from ref to binary string
	
	_as_hex(str)	return ref to scalar string containing the value as
			unsigned hex string, with the '0x' prepended.
			Leading zeros must be stripped.
	_as_bin(str)	Like as_hex, only as binary string containing only
			zeros and ones. Leading zeros must be stripped and a
			'0b' must be prepended.
	
	_rsft(obj,N,B)	shift object in base B by N 'digits' right
			For unsupported bases B, return undef to signal failure
	_lsft(obj,N,B)	shift object in base B by N 'digits' left
			For unsupported bases B, return undef to signal failure
	
	_xor(obj1,obj2)	XOR (bit-wise) object 1 with object 2
			Note: XOR, AND and OR pad with zeros if size mismatches
	_and(obj1,obj2)	AND (bit-wise) object 1 with object 2
	_or(obj1,obj2)	OR (bit-wise) object 1 with object 2

	_mod(obj,obj)	Return remainder of div of the 1st by the 2nd object
	_sqrt(obj)	return the square root of object (truncate to int)
	_fac(obj)	return factorial of object 1 (1*2*3*4..)
	_pow(obj,obj)	return object 1 to the power of object 2
	_gcd(obj,obj)	return Greatest Common Divisor of two objects
	
	_zeros(obj)	return number of trailing decimal zeros
	_modinv		return inverse modulus
	_modpow		return modulus of power ($x ** $y) % $z

Input strings come in as unsigned but with prefix (i.e. as '123', '0xabc'
or '0b1101').

Testing of input parameter validity is done by the caller, so you need not
worry about underflow (f.i. in C<_sub()>, C<_dec()>) nor about division by
zero or similar cases.

The first parameter can be modified, that includes the possibility that you
return a reference to a completely different object instead. Although keeping
the reference and just changing it's contents is prefered over creating and
returning a different reference.

Return values are always references to objects or strings. Exceptions are
C<_lsft()> and C<_rsft()>, which return undef if they can not shift the
argument. This is used to delegate shifting of bases different than the one
you can support back to Math::BigInt, which will use some generic code to
calculate the result.

=head1 WRAP YOUR OWN

If you want to port your own favourite c-lib for big numbers to the
Math::BigInt interface, you can take any of the already existing modules as
a rough guideline. You should really wrap up the latest BigInt and BigFloat
testsuites with your module, and replace in them any of the following:

	use Math::BigInt;

by this:

	use Math::BigInt lib => 'yourlib';

This way you ensure that your library really works 100% within Math::BigInt.

=head1 LICENSE
 
This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself. 

=head1 AUTHORS

Original math code by Mark Biggar, rewritten by Tels L<http://bloodgate.com/>
in late 2000, 2001.
Seperated from BigInt and shaped API with the help of John Peacock.

=head1 SEE ALSO

L<Math::BigInt>, L<Math::BigFloat>, L<Math::BigInt::BitVect>,
L<Math::BigInt::GMP>, L<Math::BigInt::Cached> and L<Math::BigInt::Pari>.

=cut
