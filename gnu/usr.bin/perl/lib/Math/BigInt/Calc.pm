package Math::BigInt::Calc;

use 5.005;
use strict;
# use warnings;	# dont use warnings for older Perls

require Exporter;
use vars qw/@ISA $VERSION/;
@ISA = qw(Exporter);

$VERSION = '0.36';

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

# Beware of things like:
# $i = $i * $y + $car; $car = int($i / $MBASE); $i = $i % $MBASE;
# This works on x86, but fails on ARM (SA1100, iPAQ) due to whoknows what
# reasons. So, use this instead (slower, but correct):
# $i = $i * $y + $car; $car = int($i / $MBASE); $i -= $MBASE * $car;

##############################################################################
# global constants, flags and accessory
 
# constants for easier life
my $nan = 'NaN';
my ($MBASE,$BASE,$RBASE,$BASE_LEN,$MAX_VAL,$BASE_LEN2,$BASE_LEN_SMALL);
my ($AND_BITS,$XOR_BITS,$OR_BITS);
my ($AND_MASK,$XOR_MASK,$OR_MASK);

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
    
    #print "BASE_LEN: $BASE_LEN MAX_VAL: $MAX_VAL BASE: $BASE RBASE: $RBASE ";
    #print "BASE_LEN_SMALL: $BASE_LEN_SMALL MBASE: $MBASE\n";

    undef &_mul;
    undef &_div;

    # $caught & 1 != 0 => cannot use MUL
    # $caught & 2 != 0 => cannot use DIV
    # The parens around ($caught & 1) were important, indeed, if we would use
    # & here.
    if ($caught == 2)				# 2
      {
      # print "# use mul\n";
      # must USE_MUL since we cannot use DIV
      *{_mul} = \&_mul_use_mul;
      *{_div} = \&_div_use_mul;
      }
    else					# 0 or 1
      {
      # print "# use div\n";
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

  use integer;

  ############################################################################
  # the next block is no longer important

  ## this below detects 15 on a 64 bit system, because after that it becomes
  ## 1e16  and not 1000000 :/ I can make it detect 18, but then I get a lot of
  ## test failures. Ugh! (Tomake detect 18: uncomment lines marked with *)

  #my $bi = 5;			# approx. 16 bit
  #$num = int('9' x $bi);
  ## $num = 99999; # *
  ## while ( ($num+$num+1) eq '1' . '9' x $bi)	# *
  #while ( int($num+$num+1) eq '1' . '9' x $bi)
  #  {
  #  $bi++; $num = int('9' x $bi);
  #  # $bi++; $num *= 10; $num += 9;	# *
  #  }
  #$bi--;				# back off one step
  # by setting them equal, we ignore the findings and use the default
  # one-size-fits-all approach from former versions
  my $bi = $e;				# XXX, this should work always

  __PACKAGE__->_base_len($e,$bi);	# set and store

  # find out how many bits _and, _or and _xor can take (old default = 16)
  # I don't think anybody has yet 128 bit scalars, so let's play safe.
  local $^W = 0;	# don't warn about 'nonportable number'
  $AND_BITS = 15; $XOR_BITS = 15; $OR_BITS = 15;

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
  # This routine modifies array x
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
  # This routine modifies array x
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

sub _mul_use_mul
  {
  # (ref to int_num_array, ref to int_num_array)
  # multiply two numbers in internal representation
  # modifies first arg, second need not be different from first
  my ($c,$xv,$yv) = @_;

  if (@$yv == 1)
    {
    # shortcut for two very short numbers (improved by Nathan Zook)
    # works also if xv and yv are the same reference, and handles also $x == 0
    if (@$xv == 1)
      {
      if (($xv->[0] *= $yv->[0]) >= $MBASE)
         {
         $xv->[0] = $xv->[0] - ($xv->[1] = int($xv->[0] * $RBASE)) * $MBASE;
         };
      return $xv;
      }
    # $x * 0 => 0
    if ($yv->[0] == 0)
      {
      @$xv = (0);
      return $xv;
      }
    # multiply a large number a by a single element one, so speed up
    my $y = $yv->[0]; my $car = 0;
    foreach my $i (@$xv)
      {
      $i = $i * $y + $car; $car = int($i * $RBASE); $i -= $car * $MBASE;
      }
    push @$xv, $car if $car != 0;
    return $xv;
    }
  # shortcut for result $x == 0 => result = 0
  return $xv if ( ((@$xv == 1) && ($xv->[0] == 0)) ); 

  # since multiplying $x with $x fails, make copy in this case
  $yv = [@$xv] if $xv == $yv;	# same references?

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
  __strip_zeros($xv);
  $xv;
  }                                                                             

sub _mul_use_div
  {
  # (ref to int_num_array, ref to int_num_array)
  # multiply two numbers in internal representation
  # modifies first arg, second need not be different from first
  my ($c,$xv,$yv) = @_;
 
  if (@$yv == 1)
    {
    # shortcut for two small numbers, also handles $x == 0
    if (@$xv == 1)
      {
      # shortcut for two very short numbers (improved by Nathan Zook)
      # works also if xv and yv are the same reference, and handles also $x == 0
      if (($xv->[0] *= $yv->[0]) >= $MBASE)
          {
          $xv->[0] =
              $xv->[0] - ($xv->[1] = int($xv->[0] / $MBASE)) * $MBASE;
          };
      return $xv;
      }
    # $x * 0 => 0
    if ($yv->[0] == 0)
      {
      @$xv = (0);
      return $xv;
      }
    # multiply a large number a by a single element one, so speed up
    my $y = $yv->[0]; my $car = 0;
    foreach my $i (@$xv)
      {
      $i = $i * $y + $car; $car = int($i / $MBASE); $i -= $car * $MBASE;
      }
    push @$xv, $car if $car != 0;
    return $xv;
    }
  # shortcut for result $x == 0 => result = 0
  return $xv if ( ((@$xv == 1) && ($xv->[0] == 0)) ); 

  # since multiplying $x with $x fails, make copy in this case
  $yv = [@$xv] if $xv == $yv;	# same references?

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
  __strip_zeros($xv);
  $xv;
  }                                                                             

sub _div_use_mul
  {
  # ref to array, ref to array, modify first array and return remainder if 
  # in list context

  # see comments in _div_use_div() for more explanations

  my ($c,$x,$yorg) = @_;
  
  # the general div algorithmn here is about O(N*N) and thus quite slow, so
  # we first check for some special cases and use shortcuts to handle them.

  # This works, because we store the numbers in a chunked format where each
  # element contains 5..7 digits (depending on system).

  # if both numbers have only one element:
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

  # if x has more than one, but y has only one element:
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

  # now x and y have more than one element

  # check whether y has more elements than x, if yet, the result will be 0
  if (@$yorg > @$x)
    {
    my $rem;
    $rem = [@$x] if wantarray;                  # make copy
    splice (@$x,1);                             # keep ref to original array
    $x->[0] = 0;                                # set to 0
    return ($x,$rem) if wantarray;              # including remainder?
    return $x;					# only x, which is [0] now
    }
  # check whether the numbers have the same number of elements, in that case
  # the result will fit into one element and can be computed efficiently
  if (@$yorg == @$x)
    {
    my $rem;
    # if $yorg has more digits than $x (it's leading element is longer than
    # the one from $x), the result will also be 0:
    if (length(int($yorg->[-1])) > length(int($x->[-1])))
      {
      $rem = [@$x] if wantarray;		# make copy
      splice (@$x,1);				# keep ref to org array
      $x->[0] = 0;				# set to 0
      return ($x,$rem) if wantarray;		# including remainder?
      return $x;
      }
    # now calculate $x / $yorg
    if (length(int($yorg->[-1])) == length(int($x->[-1])))
      {
      # same length, so make full compare, and if equal, return 1
      # hm, same lengths, but same contents? So we need to check all parts:
      my $a = 0; my $j = scalar @$x - 1;
      # manual way (abort if unequal, good for early ne)
      while ($j >= 0)
        {
        last if ($a = $x->[$j] - $yorg->[$j]); $j--;
        }
      # $a contains the result of the compare between X and Y
      # a < 0: x < y, a == 0 => x == y, a > 0: x > y
      if ($a <= 0)
        {
        if (wantarray)
	  {
          $rem = [ 0 ];			# a = 0 => x == y => rem 1
          $rem = [@$x] if $a != 0;	# a < 0 => x < y => rem = x
	  }
        splice(@$x,1);			# keep single element
        $x->[0] = 0;			# if $a < 0
        if ($a == 0)
          {
          # $x == $y
          $x->[0] = 1;
          }
        return ($x,$rem) if wantarray;
        return $x;
        }
      # $x >= $y, proceed normally
      }
    }

  # all other cases:

  my $y = [ @$yorg ];				# always make copy to preserve

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
	   if ($car = (($x->[$xi] += $y->[$yi] + $car) >= $MBASE));
	  }
	}   
      }
    pop(@$x);
    unshift(@q, $q);
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
    __strip_zeros($x);
    __strip_zeros($d);
    return ($x,$d);
    }
  @$x = @q;
  __strip_zeros($x);
  $x;
  }

sub _div_use_div
  {
  # ref to array, ref to array, modify first array and return remainder if 
  # in list context
  my ($c,$x,$yorg) = @_;

  # the general div algorithmn here is about O(N*N) and thus quite slow, so
  # we first check for some special cases and use shortcuts to handle them.

  # This works, because we store the numbers in a chunked format where each
  # element contains 5..7 digits (depending on system).

  # if both numbers have only one element:
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
  # if x has more than one, but y has only one element:
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
  # now x and y have more than one element

  # check whether y has more elements than x, if yet, the result will be 0
  if (@$yorg > @$x)
    {
    my $rem;
    $rem = [@$x] if wantarray;			# make copy
    splice (@$x,1);				# keep ref to original array
    $x->[0] = 0;				# set to 0
    return ($x,$rem) if wantarray;		# including remainder?
    return $x;					# only x, which is [0] now
    }
  # check whether the numbers have the same number of elements, in that case
  # the result will fit into one element and can be computed efficiently
  if (@$yorg == @$x)
    {
    my $rem;
    # if $yorg has more digits than $x (it's leading element is longer than
    # the one from $x), the result will also be 0:
    if (length(int($yorg->[-1])) > length(int($x->[-1])))
      {
      $rem = [@$x] if wantarray;		# make copy
      splice (@$x,1);				# keep ref to org array
      $x->[0] = 0;				# set to 0
      return ($x,$rem) if wantarray;		# including remainder?
      return $x;
      }
    # now calculate $x / $yorg
    if (length(int($yorg->[-1])) == length(int($x->[-1])))
      {
      # same length, so make full compare, and if equal, return 1
      # hm, same lengths, but same contents? So we need to check all parts:
      my $a = 0; my $j = scalar @$x - 1;
      # manual way (abort if unequal, good for early ne)
      while ($j >= 0)
        {
        last if ($a = $x->[$j] - $yorg->[$j]); $j--;
        }
      # $a contains the result of the compare between X and Y
      # a < 0: x < y, a == 0 => x == y, a > 0: x > y
      if ($a <= 0)
        {
        if (wantarray)
	  {
          $rem = [ 0 ];			# a = 0 => x == y => rem 1
          $rem = [@$x] if $a != 0;	# a < 0 => x < y => rem = x
	  }
        splice(@$x,1);			# keep single element
        $x->[0] = 0;			# if $a < 0
        if ($a == 0)
          {
          # $x == $y
          $x->[0] = 1;
          }
        return ($x,$rem) if wantarray;
        return $x;
        }
      # $x >= $y, so proceed normally
      }
    }

  # all other cases:

  my $y = [ @$yorg ];				# always make copy to preserve
 
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

  # @q will accumulate the final result, $q contains the current computed
  # part of the final result

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
	   if ($car = (($x->[$xi] += $y->[$yi] + $car) >= $MBASE));
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
    __strip_zeros($x);
    __strip_zeros($d);
    return ($x,$d);
    }
  @$x = @q;
  __strip_zeros($x);
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
  # we need only the length of the last element, since both array have the
  # same number of parts
  $lxy = length(int($cx->[-1])) - length(int($cy->[-1]));
  return -1 if $lxy < 0;
  return 1 if $lxy > 0;

  # hm, same lengths,  but same contents? So we need to check all parts:
  my $a; my $j = scalar @$cx - 1;
  # manual way (abort if unequal, good for early ne)
  while ($j >= 0)
    {
    last if ($a = $cx->[$j] - $cy->[$j]); $j--;
    }
  return 1 if $a > 0;
  return -1 if $a < 0;
  0;						# numbers are equal
  }

sub _len
  {
  # compute number of digits

  # int() because add/sub sometimes leaves strings (like '00005') instead of
  # '5' in this place, thus causing length() to report wrong length
  my $cx = $_[1];

  (@$cx-1)*$BASE_LEN+length(int($cx->[-1]));
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
  substr($elem,-$digit-1,1);
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

  # @y is a single element, but @x has more than one element
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
  my $xlen = (@$x-1)*$BASE_LEN+length(int($x->[-1]));  # len of x in digits
  if ($src > $xlen or ($src == $xlen and ! defined $x->[1]))
    {
    # 12345 67890 shifted right by more than 10 digits => 0
    splice (@$x,1);                    # leave only one element
    $x->[0] = 0;                       # set to zero
    return $x;
    }
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
  # now we must do the left over steps

  # do so as long as n has more than one element
  my $n = $cx->[0];
  # as soon as the last element of $cx is 0, we split it up and remember how
  # many zeors we got so far. The reason is that n! will accumulate zeros at
  # the end rather fast.
  my $zero_elements = 0;
  $cx = [$last];
  if (scalar @$cx == 1)
    {
    my $n = _copy($c,$cx);
    # no need to test for $steps, since $steps is a scalar and we stop before
    while (scalar @$n != 1)
      {
      if ($cx->[0] == 0)
        {
        $zero_elements ++; shift @$cx;
        }
      _mul($c,$cx,$n); _dec($c,$n);
      }
    $n = $n->[0];		# "convert" to scalar
    }
  
  # the left over steps will fit into a scalar, so we can speed it up
  while ($n != $step)
    {
    if ($cx->[0] == 0)
      {
      $zero_elements ++; shift @$cx;
      }
    _mul($c,$cx,[$n]); $n--;
    }
  # multiply in the zeros again
  while ($zero_elements-- > 0)
    {
    unshift @$cx, 0; 
    }
  $cx;
  }

# for debugging:
  use constant DEBUG => 0;
  my $steps = 0;
  sub steps { $steps };

sub _sqrt
  {
  # square-root of $x in place
  # Compute a guess of the result (by rule of thumb), then improve it via
  # Newton's method.
  my ($c,$x) = @_;

  if (scalar @$x == 1)
    {
    # fit's into one Perl scalar, so result can be computed directly
    $x->[0] = int(sqrt($x->[0]));
    return $x;
    } 
  my $y = _copy($c,$x);
  # hopefully _len/2 is < $BASE, the -1 is to always undershot the guess
  # since our guess will "grow"
  my $l = int((_len($c,$x)-1) / 2);	

  my $lastelem = $x->[-1];					# for guess
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

  splice @$x,$l;		# keep ref($x), but modify it

  # we make the first part of the guess not '1000...0' but int(sqrt($lastelem))
  # that gives us:
  # 14400 00000 => sqrt(14400) => guess first digits to be 120
  # 144000 000000 => sqrt(144000) => guess 379

  print "$lastelem (elems $elems) => " if DEBUG;
  $lastelem = $lastelem / 10 if ($elems & 1 == 1);		# odd or even?
  my $g = sqrt($lastelem); $g =~ s/\.//;			# 2.345 => 2345
  $r -= 1 if $elems & 1 == 0;					# 70 => 7

  # padd with zeros if result is too short
  $x->[$l--] = int(substr($g . '0' x $r,0,$r+1));
  print "now ",$x->[-1] if DEBUG;
  print " would have been ", int('1' . '0' x $r),"\n" if DEBUG;

  # If @$x > 1, we could compute the second elem of the guess, too, to create
  # an even better guess. Not implemented yet. Does it improve performance?
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
    print " x= ",${_str($c,$x)},"\n" if DEBUG;
    }
  print "\nsteps in sqrt: $steps, " if DEBUG;
  _dec($c,$x) if _acmp($c,$y,_mul($c,_copy($c,$x),$x)) < 0;	# overshot? 
  print " final ",$x->[-1],"\n" if DEBUG;
  $x;
  }

sub _root
  {
  # take n'th root of $x in place (n >= 3)
  my ($c,$x,$n) = @_;
 
  if (scalar @$x == 1)
    {
    if (scalar @$n > 1)
      {
      # result will always be smaller than 2 so trunc to 1 at once
      $x->[0] = 1;
      }
    else
      {
      # fit's into one Perl scalar, so result can be computed directly
      $x->[0] = int( $x->[0] ** (1 / $n->[0]) );
      }
    return $x;
    } 

  # X is more than one element
  # if $n is a power of two, we can repeatedly take sqrt($X) and find the
  # proper result, because sqrt(sqrt($x)) == root($x,4)
  my $b = _as_bin($c,$n);
  if ($$b =~ /0b1(0+)/)
    {
    my $count = CORE::length($1);	# 0b100 => len('00') => 2
    my $cnt = $count;			# counter for loop
    unshift (@$x, 0);			# add one element, together with one
					# more below in the loop this makes 2
    while ($cnt-- > 0)
      {
      # 'inflate' $X by adding one element, basically computing
      # $x * $BASE * $BASE. This gives us more $BASE_LEN digits for result
      # since len(sqrt($X)) approx == len($x) / 2.
      unshift (@$x, 0);
      # calculate sqrt($x), $x is now one element to big, again. In the next
      # round we make that two, again.
      _sqrt($c,$x);
      }
    # $x is now one element to big, so truncate result by removing it
    splice (@$x,0,1);
    } 
  else
    {
    # Should compute a guess of the result (by rule of thumb), then improve it
    # via Newton's method or something similiar.
    # XXX TODO
    warn ('_root() not fully implemented in Calc.');
    }
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

  # fit's into one element
  if (@$x == 1)
    {
    my $t = '0x' . sprintf("%x",$x->[0]);
    return \$t;
    }

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

  # fit's into one element
  if (@$x == 1)
    {
    my $t = '0b' . sprintf("%b",$x->[0]);
    return \$t;
    }
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

sub _modinv
  {
  # modular inverse
  my ($c,$x,$y) = @_;

  my $u = _zero($c); my $u1 = _one($c);
  my $a = _copy($c,$y); my $b = _copy($c,$x);

  # Euclid's Algorithm for bgcd(), only that we calc bgcd() ($a) and the
  # result ($u) at the same time. See comments in BigInt for why this works.
  my $q;
  ($a, $q, $b) = ($b, _div($c,$a,$b));		# step 1
  my $sign = 1;
  while (!_is_zero($c,$b))
    {
    my $t = _add($c, 				# step 2:
       _mul($c,_copy($c,$u1), $q) ,		#  t =  u1 * q
       $u );					#     + u
    $u = $u1;					#  u = u1, u1 = t
    $u1 = $t;
    $sign = -$sign;
    ($a, $q, $b) = ($b, _div($c,$a,$b));	# step 1
    }

  # if the gcd is not 1, then return NaN
  return (undef,undef) unless _is_one($c,$a);
 
  $sign = $sign == 1 ? '+' : '-';
  ($u1,$sign);
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
functions can also be used to support Math::BigInt, like Math::BigInt::Pari.

=head1 DESCRIPTION

In order to allow for multiple big integer libraries, Math::BigInt was
rewritten to use library modules for core math routines. Any module which
follows the same API as this can be used instead by using the following:

	use Math::BigInt lib => 'libname';

'libname' is either the long name ('Math::BigInt::Pari'), or only the short
version like 'Pari'.

=head1 STORAGE

=head1 METHODS

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
			The second operand will be not be 0, so no need to
			check for that.
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
	_sqrt(obj)	return the square root of object (truncated to int)
	_root(obj)	return the n'th (n >= 3) root of obj (truncated to int)
	_fac(obj)	return factorial of object 1 (1*2*3*4..)
	_pow(obj,obj)	return object 1 to the power of object 2
	_gcd(obj,obj)	return Greatest Common Divisor of two objects
	
	_zeros(obj)	return number of trailing decimal zeros
	_modinv		return inverse modulus
	_modpow		return modulus of power ($x ** $y) % $z

Input strings come in as unsigned but with prefix (i.e. as '123', '0xabc'
or '0b1101').

So the library needs only to deal with unsigned big integers. Testing of input
parameter validity is done by the caller, so you need not worry about
underflow (f.i. in C<_sub()>, C<_dec()>) nor about division by zero or similar
cases.

The first parameter can be modified, that includes the possibility that you
return a reference to a completely different object instead. Although keeping
the reference and just changing it's contents is prefered over creating and
returning a different reference.

Return values are always references to objects, strings, or true/false for
comparisation routines.

Exceptions are C<_lsft()> and C<_rsft()>, which return undef if they can not
shift the argument. This is used to delegate shifting of bases different than
the one you can support back to Math::BigInt, which will use some generic code
to calculate the result.

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
in late 2000.
Seperated from BigInt and shaped API with the help of John Peacock.
Fixed/enhanced by Tels 2001-2002.

=head1 SEE ALSO

L<Math::BigInt>, L<Math::BigFloat>, L<Math::BigInt::BitVect>,
L<Math::BigInt::GMP>, L<Math::BigInt::FastCalc> and L<Math::BigInt::Pari>.

=cut
