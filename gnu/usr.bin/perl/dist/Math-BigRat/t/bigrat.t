#!/usr/bin/perl -w

use strict;
use Test::More tests => 202;

# basic testing of Math::BigRat

use Math::BigRat;
use Math::BigInt;
use Math::BigFloat;

# shortcuts
my $cr = 'Math::BigRat';
my $mbi = 'Math::BigInt';
my $mbf = 'Math::BigFloat';

my ($x,$y,$z);

$x = Math::BigRat->new(1234); 		is ($x,1234);
isa_ok ($x, 'Math::BigRat');
is ($x->isa('Math::BigFloat'), 0);
is ($x->isa('Math::BigInt'), 0);

##############################################################################
# new and bnorm()

foreach my $func (qw/new bnorm/)
  {
  $x = $cr->$func(1234); 	is ($x,1234);

  $x = $cr->$func('1234/1'); 	is ($x,1234);
  $x = $cr->$func('1234/2'); 	is ($x,617);

  $x = $cr->$func('100/1.0');	is ($x,100);
  $x = $cr->$func('10.0/1.0');	is ($x,10);
  $x = $cr->$func('0.1/10');	is ($x,'1/100');
  $x = $cr->$func('0.1/0.1');	is ($x,'1');
  $x = $cr->$func('1e2/10');	is ($x,10);
  $x = $cr->$func('5/1e2');	is ($x,'1/20');
  $x = $cr->$func('1e2/1e1');	is ($x,10);
  $x = $cr->$func('1 / 3');	is ($x,'1/3');
  $x = $cr->$func('-1 / 3');	is ($x,'-1/3');
  $x = $cr->$func('NaN');	is ($x,'NaN');
  $x = $cr->$func('inf');	is ($x,'inf');
  $x = $cr->$func('-inf');	is ($x,'-inf');
  $x = $cr->$func('1/');	is ($x,'NaN');

  $x = $cr->$func("0x7e");	is ($x,126);

  # input ala '1+1/3' isn't parsed ok yet
  $x = $cr->$func('1+1/3');	is ($x,'NaN');

  $x = $cr->$func('1/1.2');	is ($x,'5/6');
  $x = $cr->$func('1.3/1.2');	is ($x,'13/12');
  $x = $cr->$func('1.2/1');	is ($x,'6/5');

  ############################################################################
  # other classes as input

  $x = $cr->$func($mbi->new(1231));	is ($x,'1231');
  $x = $cr->$func($mbf->new(1232));	is ($x,'1232');
  $x = $cr->$func($mbf->new(1232.3));	is ($x,'12323/10');
  }

my $n = 'numerator';
my $d = 'denominator';

$x =  $cr->new('-0'); is ($x,'0'); 	is ($x->$n(), '0'); is ($x->$d(),'1');
$x =  $cr->new('NaN'); is ($x,'NaN');	is ($x->$n(), 'NaN'); is ($x->$d(),'NaN');
$x =  $cr->new('-NaN'); is ($x,'NaN');	is ($x->$n(), 'NaN'); is ($x->$d(),'NaN');
$x =  $cr->new('-1r4'); is ($x,'NaN');	is ($x->$n(), 'NaN'); is ($x->$d(),'NaN');

$x =  $cr->new('+inf'); is ($x,'inf');	is ($x->$n(), 'inf'); is ($x->$d(),'1');
$x =  $cr->new('-inf'); is ($x,'-inf'); is ($x->$n(), '-inf'); is ($x->$d(),'1');
$x =  $cr->new('123a4'); is ($x,'NaN'); is ($x->$n(), 'NaN'); is ($x->$d(),'NaN');

# wrong inputs
$x =  $cr->new('1e2e2'); is ($x,'NaN'); is ($x->$n(), 'NaN'); is ($x->$d(),'NaN');
$x =  $cr->new('1+2+2'); is ($x,'NaN'); is ($x->$n(), 'NaN'); is ($x->$d(),'NaN');
# failed due to BigFloat bug
$x =  $cr->new('1.2.2'); is ($x,'NaN'); is ($x->$n(), 'NaN'); is ($x->$d(),'NaN');

is ($cr->new('123a4'),'NaN');
is ($cr->new('123e4'),'1230000');
is ($cr->new('-NaN'),'NaN');
is ($cr->new('NaN'),'NaN');
is ($cr->new('+inf'),'inf');
is ($cr->new('-inf'),'-inf');

##############################################################################
# two Bigints

is ($cr->new($mbi->new(3),$mbi->new(7))->badd(1),'10/7');
is ($cr->new($mbi->new(-13),$mbi->new(7)),'-13/7');
is ($cr->new($mbi->new(13),$mbi->new(-7)),'-13/7');
is ($cr->new($mbi->new(-13),$mbi->new(-7)),'13/7');

##############################################################################
# mixed arguments

is ($cr->new('3/7')->badd(1),'10/7');
is ($cr->new('3/10')->badd(1.1),'7/5');
is ($cr->new('3/7')->badd($mbi->new(1)),'10/7');
is ($cr->new('3/10')->badd($mbf->new('1.1')),'7/5');

is ($cr->new('3/7')->bsub(1),'-4/7');
is ($cr->new('3/10')->bsub(1.1),'-4/5');
is ($cr->new('3/7')->bsub($mbi->new(1)),'-4/7');
is ($cr->new('3/10')->bsub($mbf->new('1.1')),'-4/5');

is ($cr->new('3/7')->bmul(1),'3/7');
is ($cr->new('3/10')->bmul(1.1),'33/100');
is ($cr->new('3/7')->bmul($mbi->new(1)),'3/7');
is ($cr->new('3/10')->bmul($mbf->new('1.1')),'33/100');

is ($cr->new('3/7')->bdiv(1),'3/7');
is ($cr->new('3/10')->bdiv(1.1),'3/11');
is ($cr->new('3/7')->bdiv($mbi->new(1)),'3/7');
is ($cr->new('3/10')->bdiv($mbf->new('1.1')),'3/11');

##############################################################################
$x = $cr->new('1/4'); $y = $cr->new('1/3');

is ($x + $y, '7/12');
is ($x * $y, '1/12');
is ($x / $y, '3/4');

$x = $cr->new('7/5'); $x *= '3/2';
is ($x,'21/10');
$x -= '0.1';
is ($x,'2');	# not 21/10

$x = $cr->new('2/3');		$y = $cr->new('3/2');
is ($x > $y,'');
is ($x < $y,1);
is ($x == $y,'');

$x = $cr->new('-2/3');		$y = $cr->new('3/2');
is ($x > $y,'');
is ($x < $y,'1');
is ($x == $y,'');

$x = $cr->new('-2/3');		$y = $cr->new('-2/3');
is ($x > $y,'');
is ($x < $y,'');
is ($x == $y,'1');

$x = $cr->new('-2/3');		$y = $cr->new('-1/3');
is ($x > $y,'');
is ($x < $y,'1');
is ($x == $y,'');

$x = $cr->new('-124');		$y = $cr->new('-122');
is ($x->bacmp($y),1);

$x = $cr->new('-124');		$y = $cr->new('-122');
is ($x->bcmp($y),-1);

$x = $cr->new('3/7');		$y = $cr->new('5/7');
is ($x+$y,'8/7');

$x = $cr->new('3/7');		$y = $cr->new('5/7');
is ($x*$y,'15/49');

$x = $cr->new('3/5');		$y = $cr->new('5/7');
is ($x*$y,'3/7');

$x = $cr->new('3/5');		$y = $cr->new('5/7');
is ($x/$y,'21/25');

$x = $cr->new('7/4');		$y = $cr->new('1');
is ($x % $y,'3/4');

$x = $cr->new('7/4');		$y = $cr->new('5/13');
is ($x % $y,'11/52');

$x = $cr->new('7/4');		$y = $cr->new('5/9');
is ($x % $y,'1/12');

$x = $cr->new('-144/9')->bsqrt();	is ($x,'NaN');
$x = $cr->new('144/9')->bsqrt();	is ($x,'4');
$x = $cr->new('3/4')->bsqrt();		is ($x,
  '1732050807568877293527446341505872366943/'
 .'2000000000000000000000000000000000000000');

##############################################################################
# bpow

$x = $cr->new('2/1');  $z = $x->bpow('3/1'); is ($x,'8');
$x = $cr->new('1/2');  $z = $x->bpow('3/1'); is ($x,'1/8');
$x = $cr->new('1/3');  $z = $x->bpow('4/1'); is ($x,'1/81');
$x = $cr->new('2/3');  $z = $x->bpow('4/1'); is ($x,'16/81');

$x = $cr->new('2/3');  $z = $x->bpow('5/3');
is ($x, '31797617848703662994667839220546583581/62500000000000000000000000000000000000');

##############################################################################
# bfac

$x = $cr->new('1');  $x->bfac(); is ($x,'1');
for (my $i = 0; $i < 8; $i++)
  {
  $x = $cr->new("$i/1")->bfac(); is ($x,$mbi->new($i)->bfac());
  }

# test for $self->bnan() vs. $x->bnan();
$x = $cr->new('-1'); $x->bfac(); is ($x,'NaN');

##############################################################################
# binc/bdec

$x =  $cr->new('3/2'); is ($x->binc(),'5/2');
$x =  $cr->new('15/6'); is ($x->bdec(),'3/2');

##############################################################################
# bfloor/bceil

$x = $cr->new('-7/7'); is ($x->$n(), '-1'); is ($x->$d(), '1');
$x = $cr->new('-7/7')->bfloor(); is ($x->$n(), '-1'); is ($x->$d(), '1');

##############################################################################
# bsstr

$x = $cr->new('7/5')->bsstr(); is ($x,'7/5');
$x = $cr->new('-7/5')->bsstr(); is ($x,'-7/5');

##############################################################################
# numify()

my @array = qw/1 2 3 4 5 6 7 8 9/;
$x = $cr->new('8/8'); is ($array[$x],2);
$x = $cr->new('16/8'); is ($array[$x],3);
$x = $cr->new('17/8'); is ($array[$x],3);
$x = $cr->new('33/8'); is ($array[$x],5);
$x = $cr->new('-33/8'); is ($array[$x],6);
$x = $cr->new('-8/1'); is ($array[$x],2);	# -8 => 2

$x = $cr->new('33/8'); is ($x->numify() * 1000, 4125);
$x = $cr->new('-33/8'); is ($x->numify() * 1000, -4125);
$x = $cr->new('inf'); is ($x->numify(), 'inf');
$x = $cr->new('-inf'); is ($x->numify(), '-inf');
$x = $cr->new('NaN'); is ($x->numify(), 'NaN');

$x = $cr->new('4/3'); is ($x->numify(), 4/3);

##############################################################################
# as_hex(), as_bin(), as_oct()

$x = $cr->new('8/8');
is ($x->as_hex(), '0x1'); is ($x->as_bin(), '0b1'); is ($x->as_oct(), '01');
$x = $cr->new('80/8');
is ($x->as_hex(), '0xa'); is ($x->as_bin(), '0b1010'); is ($x->as_oct(), '012');

##############################################################################
# broot(), blog(), bmodpow() and bmodinv()

$x = $cr->new(2) ** 32;
$y = $cr->new(4);
$z = $cr->new(3);

is ($x->copy()->broot($y), 2 ** 8);
is (ref($x->copy()->broot($y)), $cr);

is ($x->copy()->bmodpow($y,$z), 1);
is (ref($x->copy()->bmodpow($y,$z)), $cr);

$x = $cr->new(8);
$y = $cr->new(5033);
$z = $cr->new(4404);

is ($x->copy()->bmodinv($y), $z);
is (ref($x->copy()->bmodinv($y)), $cr);

# square root with exact result
$x = $cr->new('1.44');
is ($x->copy()->broot(2), '6/5');
is (ref($x->copy()->broot(2)), $cr);

# log with exact result
$x = $cr->new('256.1');
is ($x->copy()->blog(2), '8000563442710106079310294693803606983661/1000000000000000000000000000000000000000');
is (ref($x->copy()->blog(2)), $cr);

$x = $cr->new(144);
is ($x->copy()->broot('2'), 12, 'v/144 = 12');

$x = $cr->new(12*12*12);
is ($x->copy()->broot('3'), 12, '(12*12*12) ** 1/3 = 12');

##############################################################################
# from_hex(), from_bin(), from_oct()

$x = Math::BigRat->from_hex('0x100');
is ($x, '256', 'from_hex');
$x = $cr->from_hex('0x100');
is ($x, '256', 'from_hex');

$x = Math::BigRat->from_bin('0b100');
is ($x, '4', 'from_bin');
$x = $cr->from_bin('0b100');
is ($x, '4', 'from_bin');

$x = Math::BigRat->from_oct('0100');
is ($x, '64', 'from_oct');
$x = $cr->from_oct('0100');
is ($x, '64', 'from_oct');

##############################################################################
# as_float()

$x = Math::BigRat->new('1/2'); my $f = $x->as_float();

is ($x, '1/2', '$x unmodified');
is ($f, '0.5', 'as_float(0.5)');

$x = Math::BigRat->new('2/3'); $f = $x->as_float(5);

is ($x, '2/3', '$x unmodified');
is ($f, '0.66667', 'as_float(2/3,5)');

##############################################################################
# int()

$x  = Math::BigRat->new('5/2');
is int($x), '2', '5/2 converted to integer';
$x  = Math::BigRat->new('-1/2');
is int($x), '0', '-1/2 converted to integer';

##############################################################################
# done

1;
