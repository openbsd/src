package Math::BigInt;

use 5.005;
use strict;
# use warnings;	# dont use warnings for older Perls

use vars qw/$VERSION/;

$VERSION = '0.02';

# See SYNOPSIS below.

my $CALC_EMU;

BEGIN
  {
  $CALC_EMU = Math::BigInt->config()->{'lib'};
  }

sub __emu_blog
  {
  my ($self,$x,$base,@r) = @_;

  return $x->bnan() if $x->is_zero() || $base->is_zero() || $base->is_one();

  my $acmp = $x->bacmp($base);
  return $x->bone('+',@r) if $acmp == 0;
  return $x->bzero(@r) if $acmp < 0 || $x->is_one();

  # blog($x,$base) ** $base + $y = $x

  # this trial multiplication is very fast, even for large counts (like for
  # 2 ** 1024, since this still requires only 1024 very fast steps
  # (multiplication of a large number by a very small number is very fast))
  # See Calc for an even faster algorightmn
  my $x_org = $x->copy();		# preserve orgx
  $x->bzero();				# keep ref to $x
  my $trial = $base->copy();
  while ($trial->bacmp($x_org) <= 0)
    {
    $trial->bmul($base); $x->binc();
    }
  $x->round(@r);
  }

sub __emu_bmodinv
  {
  my ($self,$x,$y,@r) = @_;

  my ($u, $u1) = ($self->bzero(), $self->bone());
  my ($a, $b) = ($y->copy(), $x->copy());

  # first step need always be done since $num (and thus $b) is never 0
  # Note that the loop is aligned so that the check occurs between #2 and #1
  # thus saving us one step #2 at the loop end. Typical loop count is 1. Even
  # a case with 28 loops still gains about 3% with this layout.
  my $q;
  ($a, $q, $b) = ($b, $a->bdiv($b));			# step #1
  # Euclid's Algorithm (calculate GCD of ($a,$b) in $a and also calculate
  # two values in $u and $u1, we use only $u1 afterwards)
  my $sign = 1;						# flip-flop
  while (!$b->is_zero())				# found GCD if $b == 0
    {
    # the original algorithm had:
    # ($u, $u1) = ($u1, $u->bsub($u1->copy()->bmul($q))); # step #2
    # The following creates exact the same sequence of numbers in $u1,
    # except for the sign ($u1 is now always positive). Since formerly
    # the sign of $u1 was alternating between '-' and '+', the $sign
    # flip-flop will take care of that, so that at the end of the loop
    # we have the real sign of $u1. Keeping numbers positive gains us
    # speed since badd() is faster than bsub() and makes it possible
    # to have the algorithmn in Calc for even more speed.

    ($u, $u1) = ($u1, $u->badd($u1->copy()->bmul($q)));	# step #2
    $sign = - $sign;					# flip sign

    ($a, $q, $b) = ($b, $a->bdiv($b));			# step #1 again
    }

  # If the gcd is not 1, then return NaN! It would be pointless to have
  # called bgcd to check this first, because we would then be performing
  # the same Euclidean Algorithm *twice* in case the gcd is 1.
  return $x->bnan() unless $a->is_one();

  $u1->bneg() if $sign != 1;				# need to flip?

  $u1->bmod($y);					# calc result
  $x->{value} = $u1->{value};				# and copy over to $x
  $x->{sign} = $u1->{sign};				# to modify in place
  $x->round(@r);
  }

sub __emu_bmodpow
  {
  my ($self,$num,$exp,$mod,@r) = @_;

  # in the trivial case,
  return $num->bzero(@r) if $mod->is_one();
  return $num->bone('+',@r) if $num->is_zero() or $num->is_one();

  # $num->bmod($mod);           # if $x is large, make it smaller first
  my $acc = $num->copy();       # but this is not really faster...

  $num->bone(); # keep ref to $num

  my $expbin = $exp->as_bin(); $expbin =~ s/^[-]?0b//; # ignore sign and prefix
  my $len = CORE::length($expbin);
  while (--$len >= 0)
    {
    $num->bmul($acc)->bmod($mod) if substr($expbin,$len,1) eq '1';
    $acc->bmul($acc)->bmod($mod);
    }

  $num->round(@r);
  }

sub __emu_bfac
  {
  my ($self,$x,@r) = @_;

  return $x->bone('+',@r) if $x->is_zero() || $x->is_one();     # 0 or 1 => 1

  my $n = $x->copy();
  $x->bone();
  # seems we need not to temp. clear A/P of $x since the result is the same
  my $f = $self->new(2);
  while ($f->bacmp($n) < 0)
    {
    $x->bmul($f); $f->binc();
    }
  $x->bmul($f,@r);			# last step and also round result
  }

sub __emu_bpow
  {
  my ($self,$x,$y,@r) = @_;

  return $x->bone('+',@r) if $y->is_zero();
  return $x->round(@r) if $x->is_one() || $y->is_one();
  return $x->round(@r) if $x->is_zero();  # 0**y => 0 (if not y <= 0)

  my $pow2 = $self->bone();
  my $y_bin = $y->as_bin(); $y_bin =~ s/^0b//;
  my $len = CORE::length($y_bin);
  while (--$len > 0)
    {
    $pow2->bmul($x) if substr($y_bin,$len,1) eq '1';    # is odd?
    $x->bmul($x);
    }
  $x->bmul($pow2);
  $x->round(@r) if !exists $x->{_f} || $x->{_f} & MB_NEVER_ROUND == 0;
  $x;
  }

sub __emu_band
  {
  my ($self,$x,$y,$sx,$sy,@r) = @_;

  return $x->bzero(@r) if $y->is_zero() || $x->is_zero();
  
  my $sign = 0;					# sign of result
  $sign = 1 if $sx == -1 && $sy == -1;

  my ($bx,$by);

  if ($sx == -1)				# if x is negative
    {
    # two's complement: inc and flip all "bits" in $bx
    $bx = $x->binc()->as_hex();			# -1 => 0, -2 => 1, -3 => 2 etc
    $bx =~ s/-?0x//;
    $bx =~ tr/0123456789abcdef/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  else
    {
    $bx = $x->as_hex();				# get binary representation
    $bx =~ s/-?0x//;
    $bx =~ tr/fedcba9876543210/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  if ($sy == -1)				# if y is negative
    {
    # two's complement: inc and flip all "bits" in $by
    $by = $y->copy()->binc()->as_hex();		# -1 => 0, -2 => 1, -3 => 2 etc
    $by =~ s/-?0x//;
    $by =~ tr/0123456789abcdef/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  else
    {
    $by = $y->as_hex();				# get binary representation
    $by =~ s/-?0x//;
    $by =~ tr/fedcba9876543210/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  # now we have bit-strings from X and Y, reverse them for padding
  $bx = reverse $bx;
  $by = reverse $by;

  # cut the longer string to the length of the shorter one (the result would
  # be 0 due to AND anyway)
  my $diff = CORE::length($bx) - CORE::length($by);
  if ($diff > 0)
    {
    $bx = substr($bx,0,CORE::length($by));
    }
  elsif ($diff < 0)
    {
    $by = substr($by,0,CORE::length($bx));
    }

  # and the strings together
  my $r = $bx & $by;

  # and reverse the result again
  $bx = reverse $r;

  # one of $x or $y was negative, so need to flip bits in the result
  # in both cases (one or two of them negative, or both positive) we need
  # to get the characters back.
  if ($sign == 1)
    {
    $bx =~ tr/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/0123456789abcdef/;
    }
  else
    {
    $bx =~ tr/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/fedcba9876543210/;
    }

  $bx = '0x' . $bx;
  if ($CALC_EMU->can('_from_hex'))
    {
    $x->{value} = $CALC_EMU->_from_hex( \$bx );
    }
  else
    {
    $r = $self->new($bx);
    $x->{value} = $r->{value};
    }

  # calculate sign of result
  $x->{sign} = '+';
  #$x->{sign} = '-' if $sx == $sy && $sx == -1 && !$x->is_zero();
  $x->{sign} = '-' if $sign == 1 && !$x->is_zero();

  $x->bdec() if $sign == 1;

  $x->round(@r);
  }

sub __emu_bior
  {
  my ($self,$x,$y,$sx,$sy,@r) = @_;

  return $x->round(@r) if $y->is_zero();

  my $sign = 0;					# sign of result
  $sign = 1 if ($sx == -1) || ($sy == -1);

  my ($bx,$by);

  if ($sx == -1)				# if x is negative
    {
    # two's complement: inc and flip all "bits" in $bx
    $bx = $x->binc()->as_hex();			# -1 => 0, -2 => 1, -3 => 2 etc
    $bx =~ s/-?0x//;
    $bx =~ tr/0123456789abcdef/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  else
    {
    $bx = $x->as_hex();				# get binary representation
    $bx =~ s/-?0x//;
    $bx =~ tr/fedcba9876543210/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  if ($sy == -1)				# if y is negative
    {
    # two's complement: inc and flip all "bits" in $by
    $by = $y->copy()->binc()->as_hex();		# -1 => 0, -2 => 1, -3 => 2 etc
    $by =~ s/-?0x//;
    $by =~ tr/0123456789abcdef/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  else
    {
    $by = $y->as_hex();				# get binary representation
    $by =~ s/-?0x//;
    $by =~ tr/fedcba9876543210/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  # now we have bit-strings from X and Y, reverse them for padding
  $bx = reverse $bx;
  $by = reverse $by;

  # padd the shorter string
  my $xx = "\x00"; $xx = "\x0f" if $sx == -1;
  my $yy = "\x00"; $yy = "\x0f" if $sy == -1;
  my $diff = CORE::length($bx) - CORE::length($by);
  if ($diff > 0)
    {
    $by .= $yy x $diff;
    }
  elsif ($diff < 0)
    {
    $bx .= $xx x abs($diff);
    }

  # or the strings together
  my $r = $bx | $by;

  # and reverse the result again
  $bx = reverse $r;

  # one of $x or $y was negative, so need to flip bits in the result
  # in both cases (one or two of them negative, or both positive) we need
  # to get the characters back.
  if ($sign == 1)
    {
    $bx =~ tr/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/0123456789abcdef/;
    }
  else
    {
    $bx =~ tr/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/fedcba9876543210/;
    }

  $bx = '0x' . $bx;
  if ($CALC_EMU->can('_from_hex'))
    {
    $x->{value} = $CALC_EMU->_from_hex( \$bx );
    }
  else
    {
    $r = $self->new($bx);
    $x->{value} = $r->{value};
    }

  # if one of X or Y was negative, we need to decrement result
  $x->bdec() if $sign == 1;

  $x->round(@r);
  }

sub __emu_bxor
  {
  my ($self,$x,$y,$sx,$sy,@r) = @_;

  return $x->round(@r) if $y->is_zero();

  my $sign = 0;					# sign of result
  $sign = 1 if $x->{sign} ne $y->{sign};

  my ($bx,$by);

  if ($sx == -1)				# if x is negative
    {
    # two's complement: inc and flip all "bits" in $bx
    $bx = $x->binc()->as_hex();			# -1 => 0, -2 => 1, -3 => 2 etc
    $bx =~ s/-?0x//;
    $bx =~ tr/0123456789abcdef/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  else
    {
    $bx = $x->as_hex();				# get binary representation
    $bx =~ s/-?0x//;
    $bx =~ tr/fedcba9876543210/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  if ($sy == -1)				# if y is negative
    {
    # two's complement: inc and flip all "bits" in $by
    $by = $y->copy()->binc()->as_hex();		# -1 => 0, -2 => 1, -3 => 2 etc
    $by =~ s/-?0x//;
    $by =~ tr/0123456789abcdef/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  else
    {
    $by = $y->as_hex();				# get binary representation
    $by =~ s/-?0x//;
    $by =~ tr/fedcba9876543210/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/;
    }
  # now we have bit-strings from X and Y, reverse them for padding
  $bx = reverse $bx;
  $by = reverse $by;

  # padd the shorter string
  my $xx = "\x00"; $xx = "\x0f" if $sx == -1;
  my $yy = "\x00"; $yy = "\x0f" if $sy == -1;
  my $diff = CORE::length($bx) - CORE::length($by);
  if ($diff > 0)
    {
    $by .= $yy x $diff;
    }
  elsif ($diff < 0)
    {
    $bx .= $xx x abs($diff);
    }

  # xor the strings together
  my $r = $bx ^ $by;

  # and reverse the result again
  $bx = reverse $r;

  # one of $x or $y was negative, so need to flip bits in the result
  # in both cases (one or two of them negative, or both positive) we need
  # to get the characters back.
  if ($sign == 1)
    {
    $bx =~ tr/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/0123456789abcdef/;
    }
  else
    {
    $bx =~ tr/\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x07\x06\x05\x04\x03\x02\x01\x00/fedcba9876543210/;
    }

  $bx = '0x' . $bx;
  if ($CALC_EMU->can('_from_hex'))
    {
    $x->{value} = $CALC_EMU->_from_hex( \$bx );
    }
  else
    {
    $r = $self->new($bx);
    $x->{value} = $r->{value};
    }

  # calculate sign of result
  $x->{sign} = '+';
  $x->{sign} = '-' if $sx != $sy && !$x->is_zero();

  $x->bdec() if $sign == 1;

  $x->round(@r);
  }

sub __emu_bsqrt
  {
  my ($self,$x,@r) = @_;

  # this is slow:
  return $x->round(@r) if $x->is_zero();	# 0,1 => 0,1

  return $x->bone('+',@r) if $x < 4;		# 1,2,3 => 1
  my $y = $x->copy();
  my $l = int($x->length()/2);

  $x->bone();					# keep ref($x), but modify it
  $x->blsft($l,10) if $l != 0;			# first guess: 1.('0' x (l/2))

  my $last = $self->bzero();
  my $two = $self->new(2);
  my $lastlast = $self->bzero();
  #my $lastlast = $x+$two;
  while ($last != $x && $lastlast != $x)
    {
    $lastlast = $last; $last = $x->copy();
    $x->badd($y / $x);
    $x->bdiv($two);
    }
  $x->bdec() if $x * $x > $y;			# overshot?
  $x->round(@r);
  }

sub __emu_broot
  {
  my ($self,$x,$y,@r) = @_;

  return $x->bsqrt() if $y->bacmp(2) == 0;	# 2 => square root

  # since we take at least a cubic root, and only 8 ** 1/3 >= 2 (==2):
  return $x->bone('+',@r) if $x < 8;		# $x=2..7 => 1

  my $num = $x->numify();

  if ($num <= 1000000)
    {
    $x = $self->new( int ( sprintf ("%.8f", $num ** (1 / $y->numify() ))));
    return $x->round(@r);
    }

  # if $n is a power of two, we can repeatedly take sqrt($X) and find the
  # proper result, because sqrt(sqrt($x)) == root($x,4)
  # See Calc.pm for more details
  my $b = $y->as_bin();
  if ($b =~ /0b1(0+)$/)
    {
    my $count = CORE::length($1);		# 0b100 => len('00') => 2
    my $cnt = $count;				# counter for loop
    my $shift = $self->new(6);
    $x->blsft($shift);				# add some zeros (even amount)
    while ($cnt-- > 0)
      {
      # 'inflate' $X by adding more zeros
      $x->blsft($shift);
      # calculate sqrt($x), $x is now a bit too big, again. In the next
      # round we make even bigger, again.
      $x->bsqrt($x);
      }
    # $x is still to big, so truncate result
    $x->brsft($shift);
    }
  else
    {
    # trial computation by starting with 2,4,6,8,10 etc until we overstep
    my $step;
    my $trial = $self->new(2);
    my $two = $self->new(2);
    my $s_128 = $self->new(128);

    local undef $Math::BigInt::accuracy;
    local undef $Math::BigInt::precision;

    # while still to do more than X steps
    do
      {
      $step = $self->new(2);
      while ( $trial->copy->bpow($y)->bacmp($x) < 0)
        {
        $step->bmul($two);
        $trial->badd($step);
        }

      # hit exactly?
      if ( $trial->copy->bpow($y)->bacmp($x) == 0)
        {
        $x->{value} = $trial->{value};	# make copy while preserving ref to $x
        return $x->round(@r);
        }
      # overstepped, so go back on step
      $trial->bsub($step);
      } while ($step > $s_128);

    $step = $two->copy();
    while ( $trial->copy->bpow($y)->bacmp($x) < 0)
      {
      $trial->badd($step);
      }

    # not hit exactly?
    if ( $x->bacmp( $trial->copy()->bpow($y) ) < 0)
      {
      $trial->bdec();
      }
    # copy result into $x (preserve ref)
    $x->{value} = $trial->{value};
    }
  $x->round(@r);
  }

sub __emu_as_hex
  {
  my ($self,$x,$s) = @_;

  return '0x0' if $x->is_zero();

  my $x1 = $x->copy()->babs(); my ($xr,$x10000,$h,$es);
  if ($] >= 5.006)
    {
    $x10000 = $self->new (0x10000); $h = 'h4';
    }
  else
    {
    $x10000 = $self->new (0x1000); $h = 'h3';
    }
  while (!$x1->is_zero())
    {
    ($x1, $xr) = bdiv($x1,$x10000);
    $es .= unpack($h,pack('v',$xr->numify()));
    }
  $es = reverse $es;
  $es =~ s/^[0]+//;		# strip leading zeros
  $s . '0x' . $es;
  }

sub __emu_as_bin
  {
  my ($self,$x,$s) = @_;

  return '0b0' if $x->is_zero();

  my $x1 = $x->copy()->babs(); my ($xr,$x10000,$b,$es);
  if ($] >= 5.006)
    {
    $x10000 = $self->new (0x10000); $b = 'b16';
    }
  else
    {
    $x10000 = $self->new (0x1000); $b = 'b12';
    }
  while (!$x1->is_zero())
    {
    ($x1, $xr) = bdiv($x1,$x10000);
    $es .= unpack($b,pack('v',$xr->numify()));
    }
  $es = reverse $es;
  $es =~ s/^[0]+//;   # strip leading zeros
  $s . '0b' . $es;
  }

##############################################################################
##############################################################################

1;
__END__

=head1 NAME

Math::BigInt::CalcEmu - Emulate low-level math with BigInt code

=head1 SYNOPSIS

Contains routines that emulate low-level math functions in BigInt, e.g.
optional routines the low-level math package does not provide on it's own.

Will be loaded on demand and automatically by BigInt.

Stuff here is really low-priority to optimize,
since it is far better to implement the operation in the low-level math
libary directly, possible even using a call to the native lib.

=head1 DESCRIPTION

=head1 METHODS

=head1 LICENSE
 
This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself. 

=head1 AUTHORS

(c) Tels http://bloodgate.com 2003 - based on BigInt code by
Tels from 2001-2003.

=head1 SEE ALSO

L<Math::BigInt>, L<Math::BigFloat>, L<Math::BigInt::BitVect>,
L<Math::BigInt::GMP> and L<Math::BigInt::Pari>.

=cut
