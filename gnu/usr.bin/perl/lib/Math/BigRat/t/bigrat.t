#!/usr/bin/perl -w

use strict;
use Test;

BEGIN 
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib'; # for running manually
  plan tests => 136;
  }

# testing of Math::BigRat

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
  $x = $cr->$func('1e2/1e1');	ok ($x,10);
  $x = $cr->$func('1 / 3');	ok ($x,'1/3');
  $x = $cr->$func('-1 / 3');	ok ($x,'-1/3');
  $x = $cr->$func('NaN');	ok ($x,'NaN');
  $x = $cr->$func('inf');	ok ($x,'inf');
  $x = $cr->$func('-inf');	ok ($x,'-inf');
  $x = $cr->$func('1/');	ok ($x,'NaN');

  # input ala '1+1/3' isn't parsed ok yet
  $x = $cr->$func('1+1/3');	ok ($x,'NaN');

  ############################################################################
  # other classes as input

  $x = $cr->$func($mbi->new(1231));	ok ($x,'1231');
  $x = $cr->$func($mbf->new(1232));	ok ($x,'1232');
  $x = $cr->$func($mbf->new(1232.3));	ok ($x,'12323/10');
  }
 
$x =  $cr->new('-0'); ok ($x,'0'); ok ($x->{_n}, '0'); ok ($x->{_d},'1');
$x =  $cr->new('NaN'); ok ($x,'NaN'); ok ($x->{_n}, '0'); ok ($x->{_d},'0');
$x =  $cr->new('-NaN'); ok ($x,'NaN'); ok ($x->{_n}, '0'); ok ($x->{_d},'0');
$x =  $cr->new('-1r4'); ok ($x,'NaN'); ok ($x->{_n}, '0'); ok ($x->{_d},'0');

$x =  $cr->new('+inf'); ok ($x,'inf'); ok ($x->{_n}, '0'); ok ($x->{_d},'0');
$x =  $cr->new('-inf'); ok ($x,'-inf'); ok ($x->{_n}, '0'); ok ($x->{_d},'0');
$x =  $cr->new('123a4'); ok ($x,'NaN'); ok ($x->{_n}, '0'); ok ($x->{_d},'0');

# wrong inputs
$x =  $cr->new('1e2e2'); ok ($x,'NaN'); ok ($x->{_n}, '0'); ok ($x->{_d},'0');
$x =  $cr->new('1+2+2'); ok ($x,'NaN'); ok ($x->{_n}, '0'); ok ($x->{_d},'0');
# failed due to BigFlaot bug
$x =  $cr->new('1.2.2'); ok ($x,'NaN'); ok ($x->{_n}, '0'); ok ($x->{_d},'0');

ok ($cr->new('123a4'),'NaN');
ok ($cr->new('123e4'),'1230000');
ok ($cr->new('-NaN'),'NaN');
ok ($cr->new('NaN'),'NaN');
ok ($cr->new('+inf'),'inf');
ok ($cr->new('-inf'),'-inf');

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

$x = $cr->new('-144/9')->bsqrt();	ok ($x,'NaN');
$x = $cr->new('144/9')->bsqrt();	ok ($x,'4');

##############################################################################
# bpow

$x = $cr->new('2/1');  $z = $x->bpow('3/1'); ok ($x,'8');

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

$x = $cr->new('-7/7'); ok ($x->{_n}, '1'); ok ($x->{_d}, '1');
$x = $cr->new('-7/7')->bfloor(); ok ($x->{_n}, '1'); ok ($x->{_d}, '1');

##############################################################################
# done

1;

