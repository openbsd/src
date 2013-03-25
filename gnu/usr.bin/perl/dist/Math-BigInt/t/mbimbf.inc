# test rounding, accuracy, precision and fallback, round_mode and mixing
# of classes

# Make sure you always quote any bare floating-point values, lest 123.46 will
# be stringified to 123.4599999999 due to limited float prevision.

use strict;
my ($x,$y,$z,$u,$rc);

###############################################################################
# test defaults and set/get

{
  no strict 'refs';
  is (${"$mbi\::accuracy"}, undef);
  is (${"$mbi\::precision"}, undef);
  is ($mbi->accuracy(), undef);
  is ($mbi->precision(), undef);
  is (${"$mbi\::div_scale"},40);
  is (${"$mbi\::round_mode"},'even');
  is ($mbi->round_mode(),'even');

  is (${"$mbf\::accuracy"}, undef);
  is (${"$mbf\::precision"}, undef);
  is ($mbf->precision(), undef);
  is ($mbf->precision(), undef);
  is (${"$mbf\::div_scale"},40);
  is (${"$mbf\::round_mode"},'even');
  is ($mbf->round_mode(),'even');
}

# accessors
foreach my $class ($mbi,$mbf)
  {
  is ($class->accuracy(), undef);
  is ($class->precision(), undef);
  is ($class->round_mode(),'even');
  is ($class->div_scale(),40);
   
  is ($class->div_scale(20),20);
  $class->div_scale(40); is ($class->div_scale(),40);
  
  is ($class->round_mode('odd'),'odd');
  $class->round_mode('even'); is ($class->round_mode(),'even');
  
  is ($class->accuracy(2),2);
  $class->accuracy(3); is ($class->accuracy(),3);
  is ($class->accuracy(undef), undef);

  is ($class->precision(2),2);
  is ($class->precision(-2),-2);
  $class->precision(3); is ($class->precision(),3);
  is ($class->precision(undef), undef);
  }

{
  no strict 'refs';
  # accuracy
  foreach (qw/5 42 -1 0/)
    {
    is (${"$mbf\::accuracy"} = $_,$_);
    is (${"$mbi\::accuracy"} = $_,$_);
    }
  is (${"$mbf\::accuracy"} = undef, undef);
  is (${"$mbi\::accuracy"} = undef, undef);

  # precision
  foreach (qw/5 42 -1 0/)
    {
    is (${"$mbf\::precision"} = $_,$_);
    is (${"$mbi\::precision"} = $_,$_);
    }
  is (${"$mbf\::precision"} = undef, undef);
  is (${"$mbi\::precision"} = undef, undef);

  # fallback
  foreach (qw/5 42 1/)
    {
    is (${"$mbf\::div_scale"} = $_,$_);
    is (${"$mbi\::div_scale"} = $_,$_);
    }
  # illegal values are possible for fallback due to no accessor

  # round_mode
  foreach (qw/odd even zero trunc +inf -inf/)
    {
    is (${"$mbf\::round_mode"} = $_,$_);
    is (${"$mbi\::round_mode"} = $_,$_);
    }
  ${"$mbf\::round_mode"} = 'zero';
  is (${"$mbf\::round_mode"},'zero');
  is (${"$mbi\::round_mode"},'-inf');	# from above

  # reset for further tests
  ${"$mbi\::accuracy"} = undef;
  ${"$mbi\::precision"} = undef;
  ${"$mbf\::div_scale"} = 40;
}

# local copies
$x = $mbf->new('123.456');
is ($x->accuracy(), undef);
is ($x->accuracy(5),5);
is ($x->accuracy(undef),undef, undef);
is ($x->precision(), undef);
is ($x->precision(5),5);
is ($x->precision(undef),undef, undef);

{
  no strict 'refs';
  # see if MBF changes MBIs values
  is (${"$mbi\::accuracy"} = 42,42);
  is (${"$mbf\::accuracy"} = 64,64);
  is (${"$mbi\::accuracy"},42);		# should be still 42
  is (${"$mbf\::accuracy"},64);		# should be now 64
}

###############################################################################
# see if creating a number under set A or P will round it

{
  no strict 'refs';
  ${"$mbi\::accuracy"} = 4;
  ${"$mbi\::precision"} = undef;

  is ($mbi->new(123456),123500);		# with A
  ${"$mbi\::accuracy"} = undef;
  ${"$mbi\::precision"} = 3;
  is ($mbi->new(123456),123000);		# with P

  ${"$mbf\::accuracy"} = 4;
  ${"$mbf\::precision"} = undef;
  ${"$mbi\::precision"} = undef;

  is ($mbf->new('123.456'),'123.5');	# with A
  ${"$mbf\::accuracy"} = undef;
  ${"$mbf\::precision"} = -1;
  is ($mbf->new('123.456'),'123.5');	# with P from MBF, not MBI!

  ${"$mbf\::precision"} = undef;		# reset
}

###############################################################################
# see if MBI leaves MBF's private parts alone

{
  no strict 'refs';
  ${"$mbi\::precision"} = undef; ${"$mbf\::precision"} = undef;
  ${"$mbi\::accuracy"} = 4; ${"$mbf\::accuracy"} = undef;
  is ($mbf->new('123.456'),'123.456');
  ${"$mbi\::accuracy"} = undef; 		# reset
}

###############################################################################
# see if setting accuracy/precision actually rounds the number

$x = $mbf->new('123.456'); $x->accuracy(4);   is ($x,'123.5');
$x = $mbf->new('123.456'); $x->precision(-2); is ($x,'123.46');

$x = $mbi->new(123456);    $x->accuracy(4);   is ($x,123500);
$x = $mbi->new(123456);    $x->precision(2);  is ($x,123500);

###############################################################################
# test actual rounding via round()

$x = $mbf->new('123.456');
is ($x->copy()->round(5),'123.46');
is ($x->copy()->round(4),'123.5');
is ($x->copy()->round(5,2),'NaN');
is ($x->copy()->round(undef,-2),'123.46');
is ($x->copy()->round(undef,2),120);

$x = $mbi->new('123');
is ($x->round(5,2),'NaN');

$x = $mbf->new('123.45000');
is ($x->copy()->round(undef,-1,'odd'),'123.5');

# see if rounding is 'sticky'
$x = $mbf->new('123.4567');
$y = $x->copy()->bround();		# no-op since nowhere A or P defined

is ($y,123.4567);			
$y = $x->copy()->round(5);
is ($y->accuracy(),5);
is ($y->precision(), undef);		# A has precedence, so P still unset
$y = $x->copy()->round(undef,2);
is ($y->precision(),2);
is ($y->accuracy(), undef);		# P has precedence, so A still unset

# see if setting A clears P and vice versa
$x = $mbf->new('123.4567');
is ($x,'123.4567');
is ($x->accuracy(4),4);
is ($x->precision(-2),-2);		# clear A
is ($x->accuracy(), undef);

$x = $mbf->new('123.4567');
is ($x,'123.4567');
is ($x->precision(-2),-2);
is ($x->accuracy(4),4);			# clear P
is ($x->precision(), undef);

# does copy work?
$x = $mbf->new(123.456); $x->accuracy(4); $x->precision(2);
$z = $x->copy(); is ($z->accuracy(),undef); is ($z->precision(),2);

# does $x->bdiv($y,d) work when $d > div_scale?
$x = $mbf->new('0.008'); $x->accuracy(8);

for my $e ( 4, 8, 16, 32 )
  {
  print "# Tried: $x->bdiv(3,$e)\n"
    unless is (scalar $x->copy()->bdiv(3,$e), '0.002' . ('6' x ($e-2)) . '7');
  }

# does accuracy()/precision work on zeros?
foreach my $c ($mbi,$mbf)
  {
  $x = $c->bzero(); $x->accuracy(5); is ($x->{_a},5);
  $x = $c->bzero(); $x->precision(5); is ($x->{_p},5);
  $x = $c->new(0); $x->accuracy(5); is ($x->{_a},5);
  $x = $c->new(0); $x->precision(5); is ($x->{_p},5);

  $x = $c->bzero(); $x->round(5); is ($x->{_a},5);
  $x = $c->bzero(); $x->round(undef,5); is ($x->{_p},5);
  $x = $c->new(0); $x->round(5); is ($x->{_a},5);
  $x = $c->new(0); $x->round(undef,5); is ($x->{_p},5);

  # see if trying to increasing A in bzero() doesn't do something
  $x = $c->bzero(); $x->{_a} = 3; $x->round(5); is ($x->{_a},3);
  }

###############################################################################
# test whether an opp calls objectify properly or not (or at least does what
# it should do given non-objects, w/ or w/o objectify())

foreach my $c ($mbi,$mbf)
  {
#  ${"$c\::precision"} = undef;		# reset
#  ${"$c\::accuracy"} = undef;		# reset

  is ($c->new(123)->badd(123),246);
  is ($c->badd(123,321),444);
  is ($c->badd(123,$c->new(321)),444);

  is ($c->new(123)->bsub(122),1);
  is ($c->bsub(321,123),198);
  is ($c->bsub(321,$c->new(123)),198);

  is ($c->new(123)->bmul(123),15129);
  is ($c->bmul(123,123),15129);
  is ($c->bmul(123,$c->new(123)),15129);

# is ($c->new(15129)->bdiv(123),123);
# is ($c->bdiv(15129,123),123);
# is ($c->bdiv(15129,$c->new(123)),123);

  is ($c->new(15131)->bmod(123),2);
  is ($c->bmod(15131,123),2);
  is ($c->bmod(15131,$c->new(123)),2);

  is ($c->new(2)->bpow(16),65536);
  is ($c->bpow(2,16),65536);
  is ($c->bpow(2,$c->new(16)),65536);

  is ($c->new(2**15)->brsft(1),2**14);
  is ($c->brsft(2**15,1),2**14);
  is ($c->brsft(2**15,$c->new(1)),2**14);

  is ($c->new(2**13)->blsft(1),2**14);
  is ($c->blsft(2**13,1),2**14);
  is ($c->blsft(2**13,$c->new(1)),2**14);
  }

###############################################################################
# test whether operations round properly afterwards
# These tests are not complete, since they do not exercise every "return"
# statement in the op's. But heh, it's better than nothing...

$x = $mbf->new('123.456');
$y = $mbf->new('654.321');
$x->{_a} = 5;		# $x->accuracy(5) would round $x straight away
$y->{_a} = 4;		# $y->accuracy(4) would round $x straight away

$z = $x + $y;		is ($z,'777.8');
$z = $y - $x;		is ($z,'530.9');
$z = $y * $x;		is ($z,'80780');
$z = $x ** 2;		is ($z,'15241');
$z = $x * $x;		is ($z,'15241');

# not: $z = -$x;		is ($z,'-123.46'); is ($x,'123.456');
$z = $x->copy(); $z->{_a} = 2; $z = $z / 2; is ($z,62);
$x = $mbf->new(123456); $x->{_a} = 4;
$z = $x->copy; $z++;	is ($z,123500);

$x = $mbi->new(123456);
$y = $mbi->new(654321);
$x->{_a} = 5;		# $x->accuracy(5) would round $x straight away
$y->{_a} = 4;		# $y->accuracy(4) would round $x straight away

$z = $x + $y; 		is ($z,777800);
$z = $y - $x; 		is ($z,530900);
$z = $y * $x;		is ($z,80780000000);
$z = $x ** 2;		is ($z,15241000000);
# not yet: $z = -$x;		is ($z,-123460); is ($x,123456);
$z = $x->copy; $z++;	is ($z,123460);
$z = $x->copy(); $z->{_a} = 2; $z = $z / 2; is ($z,62000);

$x = $mbi->new(123400); $x->{_a} = 4;
is ($x->bnot(),-123400);			# not -1234001

# both babs() and bneg() don't need to round, since the input will already
# be rounded (either as $x or via new($string)), and they don't change the
# value. The two tests below peek at this by using _a (illegally) directly
$x = $mbi->new(-123401); $x->{_a} = 4; is ($x->babs(),123401);
$x = $mbi->new(-123401); $x->{_a} = 4; is ($x->bneg(),123401);

# test fdiv rounding to A and R (bug in v1.48 and maybe earlier versions)
$mbf->round_mode('even');
$x = $mbf->new('740.7')->fdiv('6',4,undef,'zero'); is ($x,'123.4');

$x = $mbi->new('123456'); $y = $mbi->new('123456'); $y->{_a} = 6;
is ($x->bdiv($y),1); is ($x->{_a},6);			# carried over

$x = $mbi->new('123456'); $y = $mbi->new('123456'); $x->{_a} = 6;
is ($x->bdiv($y),1); is ($x->{_a},6);			# carried over

$x = $mbi->new('123456'); $y = $mbi->new('223456'); $y->{_a} = 6;
is ($x->bdiv($y),0); is ($x->{_a},6);			# carried over

$x = $mbi->new('123456'); $y = $mbi->new('223456'); $x->{_a} = 6;
is ($x->bdiv($y),0); is ($x->{_a},6);			# carried over

###############################################################################
# test that bop(0) does the same than bop(undef)

$x = $mbf->new('1234567890');
is ($x->copy()->bsqrt(0),$x->copy()->bsqrt(undef));
is ($x->copy->bsqrt(0),'35136.41828644462161665823116758077037159');

is ($x->{_a}, undef);

# test that bsqrt() modifies $x and does not just return something else
# (especially under BareCalc)
$z = $x->bsqrt();
is ($z,$x); is ($x,'35136.41828644462161665823116758077037159');

$x = $mbf->new('1.234567890123456789');
is ($x->copy()->bpow('0.5',0),$x->copy()->bpow('0.5',undef));
is ($x->copy()->bpow('0.5',0),$x->copy()->bsqrt(undef));
is ($x->copy()->bpow('2',0),'1.524157875323883675019051998750190521');

###############################################################################
# test (also under Bare) that bfac() rounds at last step

is ($mbi->new(12)->bfac(),'479001600');
is ($mbi->new(12)->bfac(2),'480000000');
$x = $mbi->new(12); $x->accuracy(2); is ($x->bfac(),'480000000');
$x = $mbi->new(13); $x->accuracy(2); is ($x->bfac(),'6200000000');
$x = $mbi->new(13); $x->accuracy(3); is ($x->bfac(),'6230000000');
$x = $mbi->new(13); $x->accuracy(4); is ($x->bfac(),'6227000000');
# this does 1,2,3...9,10,11,12...20
$x = $mbi->new(20); $x->accuracy(1); is ($x->bfac(),'2000000000000000000');

###############################################################################
# test bsqrt) rounding to given A/P/R (bug prior to v1.60)
$x = $mbi->new('123456')->bsqrt(2,undef); is ($x,'350');	# not 351
$x = $mbi->new('3')->bsqrt(2,undef); is ($x->accuracy(),2);

$mbi->round_mode('even'); $x = $mbi->new('126025')->bsqrt(2,undef,'+inf');
is ($x,'360');	# not 355 nor 350

$x = $mbi->new('126025')->bsqrt(undef,2); is ($x,'400');	 # not 355


###############################################################################
# test mixed arguments

$x = $mbf->new(10);
$u = $mbf->new(2.5);
$y = $mbi->new(2);

$z = $x + $y; is ($z,12); is (ref($z),$mbf);
$z = $x / $y; is ($z,5); is (ref($z),$mbf);
$z = $u * $y; is ($z,5); is (ref($z),$mbf);

$y = $mbi->new(12345);
$z = $u->copy()->bmul($y,2,undef,'odd'); is ($z,31000);
$z = $u->copy()->bmul($y,3,undef,'odd'); is ($z,30900);
$z = $u->copy()->bmul($y,undef,0,'odd'); is ($z,30863);
$z = $u->copy()->bmul($y,undef,1,'odd'); is ($z,30863);
$z = $u->copy()->bmul($y,undef,2,'odd'); is ($z,30860);
$z = $u->copy()->bmul($y,undef,3,'odd'); is ($z,30900);
$z = $u->copy()->bmul($y,undef,-1,'odd'); is ($z,30862.5);

my $warn = ''; $SIG{__WARN__} = sub { $warn = shift; };
# these should no longer warn, even tho '3.17' is a NaN in BigInt (>= returns
# now false, bug until v1.80)
$warn = ''; eval "\$z = 3.17 <= \$y"; is ($z, '');
unlike ($warn, qr/^Use of uninitialized value (\$y )?(in numeric le \(<=\) |)at/);	
$warn = ''; eval "\$z = \$y >= 3.17"; is ($z, '');
unlike ($warn, qr/^Use of uninitialized value (\$y )?(in numeric ge \(>=\) |)at/);	

# XXX TODO breakage:
# $z = $y->copy()->bmul($u,2,0,'odd'); is ($z,31000);
# $z = $y * $u; is ($z,5); is (ref($z),$mbi);
# $z = $y + $x; is ($z,12); is (ref($z),$mbi);
# $z = $y / $x; is ($z,0); is (ref($z),$mbi);

###############################################################################
# rounding in bdiv with fallback and already set A or P

{
  no strict 'refs';
  ${"$mbf\::accuracy"} = undef;
  ${"$mbf\::precision"} = undef;
  ${"$mbf\::div_scale"} = 40;
}

  $x = $mbf->new(10); $x->{_a} = 4;
  is ($x->bdiv(3),'3.333');
  is ($x->{_a},4);			# set's it since no fallback

$x = $mbf->new(10); $x->{_a} = 4; $y = $mbf->new(3);
is ($x->bdiv($y),'3.333');
is ($x->{_a},4);			# set's it since no fallback

# rounding to P of x
$x = $mbf->new(10); $x->{_p} = -2;
is ($x->bdiv(3),'3.33');

# round in div with requested P
$x = $mbf->new(10);
is ($x->bdiv(3,undef,-2),'3.33');

# round in div with requested P greater than fallback
{
  no strict 'refs';
  ${"$mbf\::div_scale"} = 5;
  $x = $mbf->new(10);
  is ($x->bdiv(3,undef,-8),'3.33333333');
  ${"$mbf\::div_scale"} = 40;
}

$x = $mbf->new(10); $y = $mbf->new(3); $y->{_a} = 4;
is ($x->bdiv($y),'3.333');
is ($x->{_a},4); is ($y->{_a},4);	# set's it since no fallback
is ($x->{_p}, undef); is ($y->{_p}, undef);

# rounding to P of y
$x = $mbf->new(10); $y = $mbf->new(3); $y->{_p} = -2;
is ($x->bdiv($y),'3.33');
is ($x->{_p},-2);
 is ($y->{_p},-2);
is ($x->{_a}, undef); is ($y->{_a}, undef);

###############################################################################
# test whether bround(-n) fails in MBF (undocumented in MBI)
eval { $x = $mbf->new(1); $x->bround(-2); };
like ($@, qr/^bround\(\) needs positive accuracy/);

# test whether rounding to higher accuracy is no-op
$x = $mbf->new(1); $x->{_a} = 4;
is ($x,'1.000');
$x->bround(6);                  # must be no-op
is ($x->{_a},4);
is ($x,'1.000');

$x = $mbi->new(1230); $x->{_a} = 3;
is ($x,'1230');
$x->bround(6);                  # must be no-op
is ($x->{_a},3);
is ($x,'1230');

# bround(n) should set _a
$x->bround(2);                  # smaller works
is ($x,'1200');
is ($x->{_a},2);
 
# bround(-n) is undocumented and only used by MBF
# bround(-n) should set _a
$x = $mbi->new(12345);
$x->bround(-1);
is ($x,'12300');
is ($x->{_a},4);
 
# bround(-n) should set _a
$x = $mbi->new(12345);
$x->bround(-2);
is ($x,'12000');
is ($x->{_a},3);
 
# bround(-n) should set _a
$x = $mbi->new(12345); $x->{_a} = 5;
$x->bround(-3);
is ($x,'10000');
is ($x->{_a},2);
 
# bround(-n) should set _a
$x = $mbi->new(12345); $x->{_a} = 5;
$x->bround(-4);
is ($x,'0');
is ($x->{_a},1);

# bround(-n) should be noop if n too big
$x = $mbi->new(12345);
$x->bround(-5);
is ($x,'0');			# scale to "big" => 0
is ($x->{_a},0);
 
# bround(-n) should be noop if n too big
$x = $mbi->new(54321);
$x->bround(-5);
is ($x,'100000');		# used by MBF to round 0.0054321 at 0.0_6_00000
is ($x->{_a},0);
 
# bround(-n) should be noop if n too big
$x = $mbi->new(54321); $x->{_a} = 5;
$x->bround(-6);
is ($x,'100000');		# no-op
is ($x->{_a},0);
 
# bround(n) should set _a
$x = $mbi->new(12345); $x->{_a} = 5;
$x->bround(5);                  # must be no-op
is ($x,'12345');
is ($x->{_a},5);
 
# bround(n) should set _a
$x = $mbi->new(12345); $x->{_a} = 5;
$x->bround(6);                  # must be no-op
is ($x,'12345');

$x = $mbf->new('0.0061'); $x->bfround(-2); is ($x,'0.01');
$x = $mbf->new('0.004'); $x->bfround(-2);  is ($x,'0.00');
$x = $mbf->new('0.005'); $x->bfround(-2);  is ($x,'0.00');

$x = $mbf->new('12345'); $x->bfround(2); is ($x,'12340');
$x = $mbf->new('12340'); $x->bfround(2); is ($x,'12340');

# MBI::bfround should clear A for negative P
$x = $mbi->new('1234'); $x->accuracy(3); $x->bfround(-2);
is ($x->{_a}, undef);

# test that bfround() and bround() work with large numbers

$x = $mbf->new(1)->bdiv(5678,undef,-63);
is ($x, '0.000176118351532229658330398027474462839027826699542092286016203');

$x = $mbf->new(1)->bdiv(5678,undef,-90);
is ($x, '0.000176118351532229658330398027474462839027826699542092286016202888340965128566396618527651');

$x = $mbf->new(1)->bdiv(5678,80);
is ($x, '0.00017611835153222965833039802747446283902782669954209228601620288834096512856639662');

###############################################################################
# rounding with already set precision/accuracy

$x = $mbf->new(1); $x->{_p} = -5;
is ($x,'1.00000');

# further rounding donw
is ($x->bfround(-2),'1.00');
is ($x->{_p},-2);

$x = $mbf->new(12345); $x->{_a} = 5;
is ($x->bround(2),'12000');
is ($x->{_a},2);

$x = $mbf->new('1.2345'); $x->{_a} = 5;
is ($x->bround(2),'1.2');
is ($x->{_a},2);

# mantissa/exponent format and A/P
$x = $mbf->new('12345.678'); $x->accuracy(4);
is ($x,'12350'); is ($x->{_a},4); is ($x->{_p}, undef);

#is ($x->{_m}->{_a}, undef); is ($x->{_e}->{_a}, undef);
#is ($x->{_m}->{_p}, undef); is ($x->{_e}->{_p}, undef);

# check for no A/P in case of fallback
# result
$x = $mbf->new(100) / 3;
is ($x->{_a}, undef); is ($x->{_p}, undef);

# result & remainder
$x = $mbf->new(100) / 3; ($x,$y) = $x->bdiv(3);
is ($x->{_a}, undef); is ($x->{_p}, undef);
is ($y->{_a}, undef); is ($y->{_p}, undef);

###############################################################################
# math with two numbers with different A and P

$x = $mbf->new(12345); $x->accuracy(4);		# '12340'
$y = $mbf->new(12345); $y->accuracy(2);		# '12000'
is ($x+$y,24000);				# 12340+12000=> 24340 => 24000

$x = $mbf->new(54321); $x->accuracy(4);		# '12340'
$y = $mbf->new(12345); $y->accuracy(3);		# '12000'
is ($x-$y,42000);				# 54320+12300=> 42020 => 42000

$x = $mbf->new('1.2345'); $x->precision(-2);	# '1.23'
$y = $mbf->new('1.2345'); $y->precision(-4);	# '1.2345'
is ($x+$y,'2.46');				# 1.2345+1.2300=> 2.4645 => 2.46

###############################################################################
# round should find and use proper class

#$x = Foo->new();
#is ($x->round($Foo::accuracy),'a' x $Foo::accuracy);
#is ($x->round(undef,$Foo::precision),'p' x $Foo::precision);
#is ($x->bfround($Foo::precision),'p' x $Foo::precision);
#is ($x->bround($Foo::accuracy),'a' x $Foo::accuracy);

###############################################################################
# find out whether _find_round_parameters is doing what's it's supposed to do

{
  no strict 'refs'; 
  ${"$mbi\::accuracy"} = undef;
  ${"$mbi\::precision"} = undef;
  ${"$mbi\::div_scale"} = 40;
  ${"$mbi\::round_mode"} = 'odd';
}

$x = $mbi->new(123);
my @params = $x->_find_round_parameters();
is (scalar @params,1);				# nothing to round

@params = $x->_find_round_parameters(1);
is (scalar @params,4);				# a=1
is ($params[0],$x);				# self
is ($params[1],1);				# a
is ($params[2], undef);				# p
is ($params[3],'odd');				# round_mode

@params = $x->_find_round_parameters(undef,2);
is (scalar @params,4);				# p=2
is ($params[0],$x);				# self
is ($params[1], undef);				# a
is ($params[2],2);				# p
is ($params[3],'odd');				# round_mode

eval { @params = $x->_find_round_parameters(undef,2,'foo'); };
like ($@, qr/^Unknown round mode 'foo'/);

@params = $x->_find_round_parameters(undef,2,'+inf');
is (scalar @params,4);				# p=2
is ($params[0],$x);				# self
is ($params[1], undef);				# a
is ($params[2],2);				# p
is ($params[3],'+inf');				# round_mode

@params = $x->_find_round_parameters(2,-2,'+inf');
is (scalar @params,1);				# error, A and P defined
is ($params[0],$x);				# self

{
  no strict 'refs';
  ${"$mbi\::accuracy"} = 1;
  @params = $x->_find_round_parameters(undef,-2);
  is (scalar @params,1);			# error, A and P defined
  is ($params[0],$x);				# self
  is ($x->is_nan(),1);				# and must be NaN

  ${"$mbi\::accuracy"} = undef;
  ${"$mbi\::precision"} = 1;
  @params = $x->_find_round_parameters(1,undef);
  is (scalar @params,1);			# error, A and P defined
  is ($params[0],$x);				# self
  is ($x->is_nan(),1);				# and must be NaN
 
  ${"$mbi\::precision"} = undef;		# reset
}

###############################################################################
# test whether bone/bzero take additional A & P, or reset it etc

foreach my $c ($mbi,$mbf)
  {
  $x = $c->new(2)->bzero(); is ($x->{_a}, undef); is ($x->{_p}, undef);
  $x = $c->new(2)->bone();  is ($x->{_a}, undef); is ($x->{_p}, undef);
  $x = $c->new(2)->binf();  is ($x->{_a}, undef); is ($x->{_p}, undef);
  $x = $c->new(2)->bnan();  is ($x->{_a}, undef); is ($x->{_p}, undef);

  $x = $c->new(2); $x->{_a} = 1; $x->{_p} = 2; $x->bnan();
  is ($x->{_a}, undef); is ($x->{_p}, undef);
  $x = $c->new(2); $x->{_a} = 1; $x->{_p} = 2; $x->binf();
  is ($x->{_a}, undef); is ($x->{_p}, undef);

  $x = $c->new(2,1); is ($x->{_a},1); is ($x->{_p}, undef);
  $x = $c->new(2,undef,1); is ($x->{_a}, undef); is ($x->{_p},1);
  
  $x = $c->new(2,1)->bzero(); is ($x->{_a},1); is ($x->{_p}, undef);
  $x = $c->new(2,undef,1)->bzero(); is ($x->{_a}, undef); is ($x->{_p},1);

  $x = $c->new(2,1)->bone(); is ($x->{_a},1); is ($x->{_p}, undef);
  $x = $c->new(2,undef,1)->bone(); is ($x->{_a}, undef); is ($x->{_p},1);

  $x = $c->new(2); $x->bone('+',2,undef); is ($x->{_a},2); is ($x->{_p}, undef);
  $x = $c->new(2); $x->bone('+',undef,2); is ($x->{_a}, undef); is ($x->{_p},2);
  $x = $c->new(2); $x->bone('-',2,undef); is ($x->{_a},2); is ($x->{_p}, undef);
  $x = $c->new(2); $x->bone('-',undef,2); is ($x->{_a}, undef); is ($x->{_p},2);
  
  $x = $c->new(2); $x->bzero(2,undef); is ($x->{_a},2); is ($x->{_p}, undef);
  $x = $c->new(2); $x->bzero(undef,2); is ($x->{_a}, undef); is ($x->{_p},2);
  }

###############################################################################
# test whether bone/bzero honour globals

for my $c ($mbi,$mbf)
  {
  $c->accuracy(2);
  $x = $c->bone(); is ($x->accuracy(),2);
  $x = $c->bzero(); is ($x->accuracy(),2);
  $c->accuracy(undef);
  
  $c->precision(-2);
  $x = $c->bone(); is ($x->precision(),-2);
  $x = $c->bzero(); is ($x->precision(),-2);
  $c->precision(undef);
  }

###############################################################################
# check whether mixing A and P creates a NaN

# new with set accuracy/precision and with parameters
{
  no strict 'refs'; 
  foreach my $c ($mbi,$mbf)
    {
    is ($c->new(123,4,-3),'NaN');			# with parameters
    ${"$c\::accuracy"} = 42;
    ${"$c\::precision"} = 2;
    is ($c->new(123),'NaN');			# with globals
    ${"$c\::accuracy"} = undef;
    ${"$c\::precision"} = undef;
    }
}

# binary ops
foreach my $class ($mbi,$mbf)
  {
  foreach (qw/add sub mul pow mod/)
  #foreach (qw/add sub mul div pow mod/)
    {
    my $try = "my \$x = $class->new(1234); \$x->accuracy(5); ";
      $try .= "my \$y = $class->new(12); \$y->precision(-3); ";
      $try .= "\$x->b$_(\$y);";
    $rc = eval $try;
    print "# Tried: '$try'\n" if !is ($rc, 'NaN');
    }
  }

# unary ops
foreach (qw/new bsqrt/)
  {
  my $try = 'my $x = $mbi->$_(1234,5,-3); ';
  $rc = eval $try;
  print "# Tried: '$try'\n" if !is ($rc, 'NaN');
  }

# see if $x->bsub(0) and $x->badd(0) really round
foreach my $class ($mbi,$mbf)
  {
  $x = $class->new(123); $class->accuracy(2); $x->bsub(0);
  is ($x,120);
  $class->accuracy(undef);
  $x = $class->new(123); $class->accuracy(2); $x->badd(0);
  is ($x,120);
  $class->accuracy(undef);
  }

###############################################################################
# test whether shortcuts returning zero/one preserve A and P

my ($ans1,$f,$a,$p,$xp,$yp,$xa,$ya,$try,$ans,@args);
my $CALC = Math::BigInt->config()->{lib};
while (<DATA>)
  {
  $_ =~ s/[\n\r]//g;	# remove newlines
  next if /^\s*(#|$)/;	# skip comments and empty lines
  if (s/^&//)
    {
    $f = $_; next;	# function
    }
  @args = split(/:/,$_,99);
  my $ans = pop(@args);

  ($x,$xa,$xp) = split (/,/,$args[0]);
  $xa = $xa || ''; $xp = $xp || '';
  $try  = "\$x = $mbi->new('$x'); ";
  $try .= "\$x->accuracy($xa); " if $xa ne '';
  $try .= "\$x->precision($xp); " if $xp ne '';

  ($y,$ya,$yp) = split (/,/,$args[1]);
  $ya = $ya || ''; $yp = $yp || '';
  $try .= "\$y = $mbi->new('$y'); ";
  $try .= "\$y->accuracy($ya); " if $ya ne '';
  $try .= "\$y->precision($yp); " if $yp ne '';
  
  $try .= "\$x->$f(\$y);";
  
  # print "trying $try\n";
  $rc = eval $try;
  # convert hex/binary targets to decimal
  if ($ans =~ /^(0x0x|0b0b)/)
    {
    $ans =~ s/^0[xb]//;
    $ans = $mbi->new($ans)->bstr();
    }
  print "# Tried: '$try'\n" if !is ($rc, $ans);
  # check internal state of number objects
  is_valid($rc,$f) if ref $rc;

  # now check whether A and P are set correctly
  # only one of $a or $p will be set (no crossing here)
  $a = $xa || $ya; $p = $xp || $yp;

  # print "Check a=$a p=$p\n";
  # print "# Tried: '$try'\n";
  if ($a ne '')
    {
    if (!(is ($x->{_a}, $a) && is ($x->{_p}, undef)))
      {
      print "# Check: A=$a and P=undef\n";
      print "# Tried: '$try'\n";
      } 
    }
  if ($p ne '')
    {
    if (!(is ($x->{_p}, $p) && is($x->{_a}, undef)))
      {
      print "# Check: A=undef and P=$p\n";
      print "# Tried: '$try'\n";
      }
    }
  }

# all done
1;

###############################################################################
# sub to check validity of a BigInt internally, to ensure that no op leaves a
# number object in an invalid state (f.i. "-0")

sub is_valid
  {
  my ($x,$f) = @_;

  my $e = 0;                    # error?
  # ok as reference?
  $e = 'Not a reference' if !ref($x);

  # has ok sign?
  $e = "Illegal sign $x->{sign} (expected: '+', '-', '-inf', '+inf' or 'NaN'"
   if $e eq '0' && $x->{sign} !~ /^(\+|-|\+inf|-inf|NaN)$/;

  $e = "-0 is invalid!" if $e ne '0' && $x->{sign} eq '-' && $x == 0;
  $e = $CALC->_check($x->{value}) if $e eq '0';

  # test done, see if error did crop up
  is (1,1), return if ($e eq '0');

  is (1,$e." after op '$f'");
  } 

# format is:
# x,A,P:x,A,P:result
# 123,,3 means 123 with precision 3 (A is undef)
# the A or P of the result is calculated automatically
__DATA__
&badd
123,,:123,,:246
123,3,:0,,:123
123,,-3:0,,:123
123,,:0,3,:123
123,,:0,,-3:123
&bmul
123,,:1,,:123
123,3,:0,,:0
123,,-3:0,,:0
123,,:0,3,:0
123,,:0,,-3:0
123,3,:1,,:123
123,,-3:1,,:123
123,,:1,3,:123
123,,:1,,-3:123
1,3,:123,,:123
1,,-3:123,,:123
1,,:123,3,:123
1,,:123,,-3:123
&bdiv
123,,:1,,:123
123,4,:1,,:123
123,,:1,4,:123
123,,:1,,-4:123
123,,-4:1,,:123
1,4,:123,,:0
1,,:123,4,:0
1,,:123,,-4:0
1,,-4:123,,:0
&band
1,,:3,,:1
1234,1,:0,,:0
1234,,:0,1,:0
1234,,-1:0,,:0
1234,,:0,,-1:0
0xFF,,:0x10,,:0x0x10
0xFF,2,:0xFF,,:250
0xFF,,:0xFF,2,:250
0xFF,,1:0xFF,,:250
0xFF,,:0xFF,,1:250
&bxor
1,,:3,,:2
1234,1,:0,,:1000
1234,,:0,1,:1000
1234,,3:0,,:1000
1234,,:0,,3:1000
0xFF,,:0x10,,:239
# 250 ^ 255 => 5
0xFF,2,:0xFF,,:5
0xFF,,:0xFF,2,:5
0xFF,,1:0xFF,,:5
0xFF,,:0xFF,,1:5
# 250 ^ 4095 = 3845 => 3800
0xFF,2,:0xFFF,,:3800
# 255 ^ 4100 = 4347 => 4300
0xFF,,:0xFFF,2,:4300
0xFF,,2:0xFFF,,:3800
# 255 ^ 4100 = 10fb => 4347 => 4300
0xFF,,:0xFFF,,2:4300
&bior
1,,:3,,:3
1234,1,:0,,:1000
1234,,:0,1,:1000
1234,,3:0,,:1000
1234,,:0,,3:1000
0xFF,,:0x10,,:0x0xFF
# FF | FA = FF => 250
250,2,:0xFF,,:250
0xFF,,:250,2,:250
0xFF,,1:0xFF,,:250
0xFF,,:0xFF,,1:250
&bpow
2,,:3,,:8
2,,:0,,:1
2,2,:0,,:1
2,,:0,2,:1
