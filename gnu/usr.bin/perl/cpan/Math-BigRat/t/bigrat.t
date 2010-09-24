#!/usr/bin/perl -w

use strict;
use Test;

BEGIN 
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib'; # for running manually
  plan tests => 200;
  }

# basic testing of Math::BigRat

use Math::BigRat;
use Math::BigInt;
use Math::BigFloat;

# shortcuts
my $cr = 'Math::BigRat';		
my $mbi = 'Math::BigInt';
my $mbf = 'Math::BigFloat';

my ($x,$y,$z);

$x = Math::BigRat->new(1234); 		ok ($x,1234);
ok ($x->isa('Math::BigRat'));
ok (!$x->isa('Math::BigFloat'));
ok (!$x->isa('Math::BigInt'));

##############################################################################
# new and bnorm()

foreach my $func (qw/new bnorm/)
  {
  $x = $cr->$func(1234); 	ok ($x,1234);

  $x = $cr->$func('1234/1'); 	ok ($x,1234);
  $x = $cr->$func('1234/2'); 	ok ($x,617);

  $x = $cr->$func('100/1.0');	ok ($x,100);
  $x = $cr->$func('10.0/1.0');	ok ($x,10);
  $x = $cr->$func('0.1/10');	ok ($x,'1/100');
  $x = $cr->$func('0.1/0.1');	ok ($x,'1');
  $x = $cr->$func('1e2/10');	ok ($x,10);
  $x = $cr->$func('5/1e2');	ok ($x,'1/20');
  $x = $cr->$func('1e2/1e1');	ok ($x,10);
  $x = $cr->$func('1 / 3');	ok ($x,'1/3');
  $x = $cr->$func('-1 / 3');	ok ($x,'-1/3');
  $x = $cr->$func('NaN');	ok ($x,'NaN');
  $x = $cr->$func('inf');	ok ($x,'inf');
  $x = $cr->$func('-inf');	ok ($x,'-inf');
  $x = $cr->$func('1/');	ok ($x,'NaN');

  $x = $cr->$func("0x7e");  ok($x, 126);

  # input ala '1+1/3' isn't parsed ok yet
  $x = $cr->$func('1+1/3');	ok ($x,'NaN');
  
  $x = $cr->$func('1/1.2');	ok ($x,'5/6');
  $x = $cr->$func('1.3/1.2');	ok ($x,'13/12');
  $x = $cr->$func('1.2/1');	ok ($x,'6/5');

  ############################################################################
  # other classes as input

  $x = $cr->$func($mbi->new(1231));	ok ($x,'1231');
  $x = $cr->$func($mbf->new(1232));	ok ($x,'1232');
  $x = $cr->$func($mbf->new(1232.3));	ok ($x,'12323/10');
  }

my $n = 'numerator';
my $d = 'denominator';

$x =  $cr->new('-0'); ok ($x,'0'); 	ok ($x->$n(), '0'); ok ($x->$d(),'1');
$x =  $cr->new('NaN'); ok ($x,'NaN');	ok ($x->$n(), 'NaN'); ok ($x->$d(),'NaN');
$x =  $cr->new('-NaN'); ok ($x,'NaN');	ok ($x->$n(), 'NaN'); ok ($x->$d(),'NaN');
$x =  $cr->new('-1r4'); ok ($x,'NaN');	ok ($x->$n(), 'NaN'); ok ($x->$d(),'NaN');

$x =  $cr->new('+inf'); ok ($x,'inf');	ok ($x->$n(), 'inf'); ok ($x->$d(),'1');
$x =  $cr->new('-inf'); ok ($x,'-inf'); ok ($x->$n(), '-inf'); ok ($x->$d(),'1');
$x =  $cr->new('123a4'); ok ($x,'NaN'); ok ($x->$n(), 'NaN'); ok ($x->$d(),'NaN');

# wrong inputs
$x =  $cr->new('1e2e2'); ok ($x,'NaN'); ok ($x->$n(), 'NaN'); ok ($x->$d(),'NaN');
$x =  $cr->new('1+2+2'); ok ($x,'NaN'); ok ($x->$n(), 'NaN'); ok ($x->$d(),'NaN');
# failed due to BigFloat bug
$x =  $cr->new('1.2.2'); ok ($x,'NaN'); ok ($x->$n(), 'NaN'); ok ($x->$d(),'NaN');

ok ($cr->new('123a4'),'NaN');
ok ($cr->new('123e4'),'1230000');
ok ($cr->new('-NaN'),'NaN');
ok ($cr->new('NaN'),'NaN');
ok ($cr->new('+inf'),'inf');
ok ($cr->new('-inf'),'-inf');

##############################################################################
# two Bigints

ok ($cr->new($mbi->new(3),$mbi->new(7))->badd(1),'10/7');
ok ($cr->new($mbi->new(-13),$mbi->new(7)),'-13/7');
ok ($cr->new($mbi->new(13),$mbi->new(-7)),'-13/7');
ok ($cr->new($mbi->new(-13),$mbi->new(-7)),'13/7');

##############################################################################
# mixed arguments

ok ($cr->new('3/7')->badd(1),'10/7');
ok ($cr->new('3/10')->badd(1.1),'7/5');
ok ($cr->new('3/7')->badd($mbi->new(1)),'10/7');
ok ($cr->new('3/10')->badd($mbf->new('1.1')),'7/5');

ok ($cr->new('3/7')->bsub(1),'-4/7');
ok ($cr->new('3/10')->bsub(1.1),'-4/5');
ok ($cr->new('3/7')->bsub($mbi->new(1)),'-4/7');
ok ($cr->new('3/10')->bsub($mbf->new('1.1')),'-4/5');

ok ($cr->new('3/7')->bmul(1),'3/7');
ok ($cr->new('3/10')->bmul(1.1),'33/100');
ok ($cr->new('3/7')->bmul($mbi->new(1)),'3/7');
ok ($cr->new('3/10')->bmul($mbf->new('1.1')),'33/100');

ok ($cr->new('3/7')->bdiv(1),'3/7');
ok ($cr->new('3/10')->bdiv(1.1),'3/11');
ok ($cr->new('3/7')->bdiv($mbi->new(1)),'3/7');
ok ($cr->new('3/10')->bdiv($mbf->new('1.1')),'3/11');

##############################################################################
$x = $cr->new('1/4'); $y = $cr->new('1/3');

ok ($x + $y, '7/12');
ok ($x * $y, '1/12');
ok ($x / $y, '3/4');

$x = $cr->new('7/5'); $x *= '3/2'; 
ok ($x,'21/10');
$x -= '0.1';
ok ($x,'2');	# not 21/10

$x = $cr->new('2/3');		$y = $cr->new('3/2');
ok ($x > $y,'');		
ok ($x < $y,1);
ok ($x == $y,'');

$x = $cr->new('-2/3');		$y = $cr->new('3/2');
ok ($x > $y,'');		
ok ($x < $y,'1');
ok ($x == $y,'');

$x = $cr->new('-2/3');		$y = $cr->new('-2/3');
ok ($x > $y,'');		
ok ($x < $y,'');
ok ($x == $y,'1');

$x = $cr->new('-2/3');		$y = $cr->new('-1/3');
ok ($x > $y,'');		
ok ($x < $y,'1');
ok ($x == $y,'');

$x = $cr->new('-124');		$y = $cr->new('-122');
ok ($x->bacmp($y),1);

$x = $cr->new('-124');		$y = $cr->new('-122');
ok ($x->bcmp($y),-1);

$x = $cr->new('3/7');		$y = $cr->new('5/7');
ok ($x+$y,'8/7');

$x = $cr->new('3/7');		$y = $cr->new('5/7');
ok ($x*$y,'15/49');

$x = $cr->new('3/5');		$y = $cr->new('5/7');
ok ($x*$y,'3/7');

$x = $cr->new('3/5');		$y = $cr->new('5/7');
ok ($x/$y,'21/25');

$x = $cr->new('7/4');		$y = $cr->new('1');
ok ($x % $y,'3/4');

$x = $cr->new('7/4');		$y = $cr->new('5/13');
ok ($x % $y,'11/52');

$x = $cr->new('7/4');		$y = $cr->new('5/9');
ok ($x % $y,'1/12');

$x = $cr->new('-144/9')->bsqrt();	ok ($x,'NaN');
$x = $cr->new('144/9')->bsqrt();	ok ($x,'4');
$x = $cr->new('3/4')->bsqrt();		ok ($x,
  '1732050807568877293527446341505872366943/'
 .'2000000000000000000000000000000000000000');

##############################################################################
# bpow

$x = $cr->new('2/1');  $z = $x->bpow('3/1'); ok ($x,'8');
$x = $cr->new('1/2');  $z = $x->bpow('3/1'); ok ($x,'1/8');
$x = $cr->new('1/3');  $z = $x->bpow('4/1'); ok ($x,'1/81');
$x = $cr->new('2/3');  $z = $x->bpow('4/1'); ok ($x,'16/81');

$x = $cr->new('2/3');  $z = $x->bpow('5/3'); 
ok ($x, '31797617848703662994667839220546583581/62500000000000000000000000000000000000');

##############################################################################
# bfac

$x = $cr->new('1');  $x->bfac(); ok ($x,'1');
for (my $i = 0; $i < 8; $i++)
  {
  $x = $cr->new("$i/1")->bfac(); ok ($x,$mbi->new($i)->bfac());
  }

# test for $self->bnan() vs. $x->bnan();
$x = $cr->new('-1'); $x->bfac(); ok ($x,'NaN');	

##############################################################################
# binc/bdec

$x =  $cr->new('3/2'); ok ($x->binc(),'5/2');
$x =  $cr->new('15/6'); ok ($x->bdec(),'3/2');

##############################################################################
# bfloor/bceil

$x = $cr->new('-7/7'); ok ($x->$n(), '-1'); ok ($x->$d(), '1');
$x = $cr->new('-7/7')->bfloor(); ok ($x->$n(), '-1'); ok ($x->$d(), '1');

##############################################################################
# bsstr

$x = $cr->new('7/5')->bsstr(); ok ($x,'7/5');
$x = $cr->new('-7/5')->bsstr(); ok ($x,'-7/5');

##############################################################################
# numify()

my @array = qw/1 2 3 4 5 6 7 8 9/;
$x = $cr->new('8/8'); ok ($array[$x],2);
$x = $cr->new('16/8'); ok ($array[$x],3);
$x = $cr->new('17/8'); ok ($array[$x],3);
$x = $cr->new('33/8'); ok ($array[$x],5);
$x = $cr->new('-33/8'); ok ($array[$x],6);
$x = $cr->new('-8/1'); ok ($array[$x],2);	# -8 => 2

$x = $cr->new('33/8'); ok ($x->numify() * 1000, 4125);
$x = $cr->new('-33/8'); ok ($x->numify() * 1000, -4125);
$x = $cr->new('inf'); ok ($x->numify(), 'inf');
$x = $cr->new('-inf'); ok ($x->numify(), '-inf');
$x = $cr->new('NaN'); ok ($x->numify(), 'NaN');

$x = $cr->new('4/3'); ok ($x->numify(), 4/3);

##############################################################################
# as_hex(), as_bin(), as_oct()

$x = $cr->new('8/8');
ok ($x->as_hex(), '0x1'); ok ($x->as_bin(), '0b1'); ok ($x->as_oct(), '01');
$x = $cr->new('80/8');
ok ($x->as_hex(), '0xa'); ok ($x->as_bin(), '0b1010'); ok ($x->as_oct(), '012');

##############################################################################
# broot(), blog(), bmodpow() and bmodinv()

$x = $cr->new(2) ** 32;
$y = $cr->new(4);
$z = $cr->new(3);

ok ($x->copy()->broot($y), 2 ** 8);
ok (ref($x->copy()->broot($y)), $cr);

ok ($x->copy()->bmodpow($y,$z), 1);
ok (ref($x->copy()->bmodpow($y,$z)), $cr);

$x = $cr->new(8);
$y = $cr->new(5033);
$z = $cr->new(4404);

ok ($x->copy()->bmodinv($y), $z);
ok (ref($x->copy()->bmodinv($y)), $cr);

# square root with exact result
$x = $cr->new('1.44');
ok ($x->copy()->broot(2), '6/5');
ok (ref($x->copy()->broot(2)), $cr);

# log with exact result
$x = $cr->new('256.1');
ok ($x->copy()->blog(2), '8000563442710106079310294693803606983661/1000000000000000000000000000000000000000');
ok (ref($x->copy()->blog(2)), $cr);

$x = $cr->new(144);
ok ($x->copy()->broot('2'), 12, 'v/144 = 12');

$x = $cr->new(12*12*12);
ok ($x->copy()->broot('3'), 12, '(12*12*12) ** 1/3 = 12');

##############################################################################
# from_hex(), from_bin(), from_oct()

$x = Math::BigRat->from_hex('0x100');
ok ($x, '256', 'from_hex');
$x = $cr->from_hex('0x100');
ok ($x, '256', 'from_hex');

$x = Math::BigRat->from_bin('0b100');
ok ($x, '4', 'from_bin');
$x = $cr->from_bin('0b100');
ok ($x, '4', 'from_bin');

$x = Math::BigRat->from_oct('0100');
ok ($x, '64', 'from_oct');
$x = $cr->from_oct('0100');
ok ($x, '64', 'from_oct');

##############################################################################
# as_float()

$x = Math::BigRat->new('1/2'); my $f = $x->as_float();

ok ($x, '1/2', '$x unmodified');
ok ($f, '0.5', 'as_float(0.5)');

$x = Math::BigRat->new('2/3'); $f = $x->as_float(5);

ok ($x, '2/3', '$x unmodified');
ok ($f, '0.66667', 'as_float(2/3,5)');

##############################################################################
# done

1;

