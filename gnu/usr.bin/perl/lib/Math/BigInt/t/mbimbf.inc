# test rounding, accuracy, precicion and fallback, round_mode and mixing
# of classes

# Make sure you always quote any bare floating-point values, lest 123.46 will
# be stringified to 123.4599999999 due to limited float prevision.

my ($x,$y,$z,$u,$rc);

###############################################################################
# test defaults and set/get

ok_undef (${"$mbi\::accuracy"});
ok_undef (${"$mbi\::precision"});
ok_undef ($mbi->accuracy());
ok_undef ($mbi->precision());
ok (${"$mbi\::div_scale"},40);
ok (${"$mbi\::round_mode"},'even');
ok ($mbi->round_mode(),'even');

ok_undef (${"$mbf\::accuracy"});
ok_undef (${"$mbf\::precision"});
ok_undef ($mbf->precision());
ok_undef ($mbf->precision());
ok (${"$mbf\::div_scale"},40);
ok (${"$mbf\::round_mode"},'even');
ok ($mbf->round_mode(),'even');

# accessors
foreach my $class ($mbi,$mbf)
  {
  ok_undef ($class->accuracy());
  ok_undef ($class->precision());
  ok ($class->round_mode(),'even');
  ok ($class->div_scale(),40);
   
  ok ($class->div_scale(20),20);
  $class->div_scale(40); ok ($class->div_scale(),40);
  
  ok ($class->round_mode('odd'),'odd');
  $class->round_mode('even'); ok ($class->round_mode(),'even');
  
  ok ($class->accuracy(2),2);
  $class->accuracy(3); ok ($class->accuracy(),3);
  ok_undef ($class->accuracy(undef));

  ok ($class->precision(2),2);
  ok ($class->precision(-2),-2);
  $class->precision(3); ok ($class->precision(),3);
  ok_undef ($class->precision(undef));
  }

# accuracy
foreach (qw/5 42 -1 0/)
  {
  ok (${"$mbf\::accuracy"} = $_,$_);
  ok (${"$mbi\::accuracy"} = $_,$_);
  }
ok_undef (${"$mbf\::accuracy"} = undef);
ok_undef (${"$mbi\::accuracy"} = undef);

# precision
foreach (qw/5 42 -1 0/)
  {
  ok (${"$mbf\::precision"} = $_,$_);
  ok (${"$mbi\::precision"} = $_,$_);
  }
ok_undef (${"$mbf\::precision"} = undef);
ok_undef (${"$mbi\::precision"} = undef);

# fallback
foreach (qw/5 42 1/)
  {
  ok (${"$mbf\::div_scale"} = $_,$_);
  ok (${"$mbi\::div_scale"} = $_,$_);
  }
# illegal values are possible for fallback due to no accessor

# round_mode
foreach (qw/odd even zero trunc +inf -inf/)
  {
  ok (${"$mbf\::round_mode"} = $_,$_);
  ok (${"$mbi\::round_mode"} = $_,$_);
  }
${"$mbf\::round_mode"} = 'zero';
ok (${"$mbf\::round_mode"},'zero');
ok (${"$mbi\::round_mode"},'-inf');	# from above

${"$mbi\::accuracy"} = undef;
${"$mbi\::precision"} = undef;
# local copies
$x = $mbf->new('123.456');
ok_undef ($x->accuracy());
ok ($x->accuracy(5),5);
ok_undef ($x->accuracy(undef),undef);
ok_undef ($x->precision());
ok ($x->precision(5),5);
ok_undef ($x->precision(undef),undef);

# see if MBF changes MBIs values
ok (${"$mbi\::accuracy"} = 42,42);
ok (${"$mbf\::accuracy"} = 64,64);
ok (${"$mbi\::accuracy"},42);		# should be still 42
ok (${"$mbf\::accuracy"},64);		# should be now 64

###############################################################################
# see if creating a number under set A or P will round it

${"$mbi\::accuracy"} = 4;
${"$mbi\::precision"} = undef;

ok ($mbi->new(123456),123500);		# with A
${"$mbi\::accuracy"} = undef;
${"$mbi\::precision"} = 3;
ok ($mbi->new(123456),123000);		# with P

${"$mbf\::accuracy"} = 4;
${"$mbf\::precision"} = undef;
${"$mbi\::precision"} = undef;

ok ($mbf->new('123.456'),'123.5');	# with A
${"$mbf\::accuracy"} = undef;
${"$mbf\::precision"} = -1;
ok ($mbf->new('123.456'),'123.5');	# with P from MBF, not MBI!

${"$mbf\::precision"} = undef;		# reset

###############################################################################
# see if MBI leaves MBF's private parts alone

${"$mbi\::precision"} = undef; ${"$mbf\::precision"} = undef;
${"$mbi\::accuracy"} = 4; ${"$mbf\::accuracy"} = undef;
ok ($mbf->new('123.456'),'123.456');
${"$mbi\::accuracy"} = undef; 		# reset

###############################################################################
# see if setting accuracy/precision actually rounds the number

$x = $mbf->new('123.456'); $x->accuracy(4);   ok ($x,'123.5');
$x = $mbf->new('123.456'); $x->precision(-2); ok ($x,'123.46');

$x = $mbi->new(123456);    $x->accuracy(4);   ok ($x,123500);
$x = $mbi->new(123456);    $x->precision(2);  ok ($x,123500);

###############################################################################
# test actual rounding via round()

$x = $mbf->new('123.456');
ok ($x->copy()->round(5),'123.46');
ok ($x->copy()->round(4),'123.5');
ok ($x->copy()->round(5,2),'NaN');
ok ($x->copy()->round(undef,-2),'123.46');
ok ($x->copy()->round(undef,2),120);

$x = $mbi->new('123');
ok ($x->round(5,2),'NaN');

$x = $mbf->new('123.45000');
ok ($x->copy()->round(undef,-1,'odd'),'123.5');

# see if rounding is 'sticky'
$x = $mbf->new('123.4567');
$y = $x->copy()->bround();		# no-op since nowhere A or P defined

ok ($y,123.4567);			
$y = $x->copy()->round(5);
ok ($y->accuracy(),5);
ok_undef ($y->precision());		# A has precedence, so P still unset
$y = $x->copy()->round(undef,2);
ok ($y->precision(),2);
ok_undef ($y->accuracy());		# P has precedence, so A still unset

# see if setting A clears P and vice versa
$x = $mbf->new('123.4567');
ok ($x,'123.4567');
ok ($x->accuracy(4),4);
ok ($x->precision(-2),-2);		# clear A
ok_undef ($x->accuracy());

$x = $mbf->new('123.4567');
ok ($x,'123.4567');
ok ($x->precision(-2),-2);
ok ($x->accuracy(4),4);			# clear P
ok_undef ($x->precision());

# does copy work?
$x = $mbf->new(123.456); $x->accuracy(4); $x->precision(2);
$z = $x->copy(); ok_undef ($z->accuracy(),undef); ok ($z->precision(),2);

# does accuracy()/precision work on zeros?
foreach my $class ($mbi,$mbf)
  {
  $x = $class->bzero(); $x->accuracy(5); ok ($x->{_a},5);
  $x = $class->bzero(); $x->precision(5); ok ($x->{_p},5);
  $x = $class->new(0); $x->accuracy(5); ok ($x->{_a},5);
  $x = $class->new(0); $x->precision(5); ok ($x->{_p},5);

  $x = $class->bzero(); $x->round(5); ok ($x->{_a},5);
  $x = $class->bzero(); $x->round(undef,5); ok ($x->{_p},5);
  $x = $class->new(0); $x->round(5); ok ($x->{_a},5);
  $x = $class->new(0); $x->round(undef,5); ok ($x->{_p},5);

  # see if trying to increasing A in bzero() doesn't do something
  $x = $class->bzero(); $x->{_a} = 3; $x->round(5); ok ($x->{_a},3);
  }

###############################################################################
# test wether operations round properly afterwards
# These tests are not complete, since they do not excercise every "return"
# statement in the op's. But heh, it's better than nothing...

$x = $mbf->new('123.456');
$y = $mbf->new('654.321');
$x->{_a} = 5;		# $x->accuracy(5) would round $x straightaway
$y->{_a} = 4;		# $y->accuracy(4) would round $x straightaway

$z = $x + $y;		ok ($z,'777.8');
$z = $y - $x;		ok ($z,'530.9');
$z = $y * $x;		ok ($z,'80780');
$z = $x ** 2;		ok ($z,'15241');
$z = $x * $x;		ok ($z,'15241');

# not: $z = -$x;		ok ($z,'-123.46'); ok ($x,'123.456');
$z = $x->copy(); $z->{_a} = 2; $z = $z / 2; ok ($z,62);
$x = $mbf->new(123456); $x->{_a} = 4;
$z = $x->copy; $z++;	ok ($z,123500);

$x = $mbi->new(123456);
$y = $mbi->new(654321);
$x->{_a} = 5;		# $x->accuracy(5) would round $x straightaway
$y->{_a} = 4;		# $y->accuracy(4) would round $x straightaway

$z = $x + $y; 		ok ($z,777800);
$z = $y - $x; 		ok ($z,530900);
$z = $y * $x;		ok ($z,80780000000);
$z = $x ** 2;		ok ($z,15241000000);
# not yet: $z = -$x;		ok ($z,-123460); ok ($x,123456);
$z = $x->copy; $z++;	ok ($z,123460);
$z = $x->copy(); $z->{_a} = 2; $z = $z / 2; ok ($z,62000);

$x = $mbi->new(123400); $x->{_a} = 4;
ok ($x->bnot(),-123400);			# not -1234001

# both babs() and bneg() don't need to round, since the input will already
# be rounded (either as $x or via new($string)), and they don't change the
# value. The two tests below peek at this by using _a (illegally) directly
$x = $mbi->new(-123401); $x->{_a} = 4; ok ($x->babs(),123401);
$x = $mbi->new(-123401); $x->{_a} = 4; ok ($x->bneg(),123401);

# test fdiv rounding to A and R (bug in v1.48 and maybe earlier versions)
$mbf->round_mode('even');
$x = $mbf->new('740.7')->fdiv('6',4,undef,'zero'); ok ($x,'123.4');

###############################################################################
# test (also under Bare) that bfac() rounds at last step

ok ($mbi->new(12)->bfac(),'479001600');
ok ($mbi->new(12)->bfac(2),'480000000');
$x = $mbi->new(12); $x->accuracy(2); ok ($x->bfac(),'480000000');
$x = $mbi->new(13); $x->accuracy(2); ok ($x->bfac(),'6200000000');
$x = $mbi->new(13); $x->accuracy(3); ok ($x->bfac(),'6230000000');
$x = $mbi->new(13); $x->accuracy(4); ok ($x->bfac(),'6227000000');
# this does 1,2,3...9,10,11,12...20
$x = $mbi->new(20); $x->accuracy(1); ok ($x->bfac(),'2000000000000000000');

###############################################################################
# test bsqrt) rounding to given A/P/R (bug prior to v1.60)
$x = $mbi->new('123456')->bsqrt(2,undef); ok ($x,'350');	# not 351
$x = $mbi->new('3')->bsqrt(2,undef); ok ($x->accuracy(),2);

$mbi->round_mode('even'); $x = $mbi->new('126025')->bsqrt(2,undef,'+inf');
ok ($x,'360');	# not 355 nor 350

$x = $mbi->new('126025')->bsqrt(undef,2); ok ($x,'400');	 # not 355


###############################################################################
# test mixed arguments

$x = $mbf->new(10);
$u = $mbf->new(2.5);
$y = $mbi->new(2);

$z = $x + $y; ok ($z,12); ok (ref($z),$mbf);
$z = $x / $y; ok ($z,5); ok (ref($z),$mbf);
$z = $u * $y; ok ($z,5); ok (ref($z),$mbf);

$y = $mbi->new(12345);
$z = $u->copy()->bmul($y,2,undef,'odd'); ok ($z,31000);
$z = $u->copy()->bmul($y,3,undef,'odd'); ok ($z,30900);
$z = $u->copy()->bmul($y,undef,0,'odd'); ok ($z,30863);
$z = $u->copy()->bmul($y,undef,1,'odd'); ok ($z,30863);
$z = $u->copy()->bmul($y,undef,2,'odd'); ok ($z,30860);
$z = $u->copy()->bmul($y,undef,3,'odd'); ok ($z,30900);
$z = $u->copy()->bmul($y,undef,-1,'odd'); ok ($z,30862.5);

# breakage:
# $z = $y->copy()->bmul($u,2,0,'odd'); ok ($z,31000);
# $z = $y * $u; ok ($z,5); ok (ref($z),$mbi);
# $z = $y + $x; ok ($z,12); ok (ref($z),$mbi);
# $z = $y / $x; ok ($z,0); ok (ref($z),$mbi);

###############################################################################
# rounding in bdiv with fallback and already set A or P

${"$mbf\::accuracy"} = undef;
${"$mbf\::precision"} = undef;
${"$mbf\::div_scale"} = 40;

$x = $mbf->new(10); $x->{_a} = 4;
ok ($x->bdiv(3),'3.333');
ok ($x->{_a},4);			# set's it since no fallback

$x = $mbf->new(10); $x->{_a} = 4; $y = $mbf->new(3);
ok ($x->bdiv($y),'3.333');
ok ($x->{_a},4);			# set's it since no fallback

# rounding to P of x
$x = $mbf->new(10); $x->{_p} = -2;
ok ($x->bdiv(3),'3.33');

# round in div with requested P
$x = $mbf->new(10);
ok ($x->bdiv(3,undef,-2),'3.33');

# round in div with requested P greater than fallback
${"$mbf\::div_scale"} = 5;
$x = $mbf->new(10);
ok ($x->bdiv(3,undef,-8),'3.33333333');
${"$mbf\::div_scale"} = 40;

$x = $mbf->new(10); $y = $mbf->new(3); $y->{_a} = 4;
ok ($x->bdiv($y),'3.333');
ok ($x->{_a},4); ok ($y->{_a},4);	# set's it since no fallback
ok_undef ($x->{_p}); ok_undef ($y->{_p});

# rounding to P of y
$x = $mbf->new(10); $y = $mbf->new(3); $y->{_p} = -2;
ok ($x->bdiv($y),'3.33');
ok ($x->{_p},-2);
 ok ($y->{_p},-2);
ok_undef ($x->{_a}); ok_undef ($y->{_a});

###############################################################################
# test whether bround(-n) fails in MBF (undocumented in MBI)
eval { $x = $mbf->new(1); $x->bround(-2); };
ok ($@ =~ /^bround\(\) needs positive accuracy/,1);

# test whether rounding to higher accuracy is no-op
$x = $mbf->new(1); $x->{_a} = 4;
ok ($x,'1.000');
$x->bround(6);                  # must be no-op
ok ($x->{_a},4);
ok ($x,'1.000');

$x = $mbi->new(1230); $x->{_a} = 3;
ok ($x,'1230');
$x->bround(6);                  # must be no-op
ok ($x->{_a},3);
ok ($x,'1230');

# bround(n) should set _a
$x->bround(2);                  # smaller works
ok ($x,'1200');
ok ($x->{_a},2);
 
# bround(-n) is undocumented and only used by MBF
# bround(-n) should set _a
$x = $mbi->new(12345);
$x->bround(-1);
ok ($x,'12300');
ok ($x->{_a},4);
 
# bround(-n) should set _a
$x = $mbi->new(12345);
$x->bround(-2);
ok ($x,'12000');
ok ($x->{_a},3);
 
# bround(-n) should set _a
$x = $mbi->new(12345); $x->{_a} = 5;
$x->bround(-3);
ok ($x,'10000');
ok ($x->{_a},2);
 
# bround(-n) should set _a
$x = $mbi->new(12345); $x->{_a} = 5;
$x->bround(-4);
ok ($x,'0');
ok ($x->{_a},1);

# bround(-n) should be noop if n too big
$x = $mbi->new(12345);
$x->bround(-5);
ok ($x,'0');			# scale to "big" => 0
ok ($x->{_a},0);
 
# bround(-n) should be noop if n too big
$x = $mbi->new(54321);
$x->bround(-5);
ok ($x,'100000');		# used by MBF to round 0.0054321 at 0.0_6_00000
ok ($x->{_a},0);
 
# bround(-n) should be noop if n too big
$x = $mbi->new(54321); $x->{_a} = 5;
$x->bround(-6);
ok ($x,'100000');		# no-op
ok ($x->{_a},0);
 
# bround(n) should set _a
$x = $mbi->new(12345); $x->{_a} = 5;
$x->bround(5);                  # must be no-op
ok ($x,'12345');
ok ($x->{_a},5);
 
# bround(n) should set _a
$x = $mbi->new(12345); $x->{_a} = 5;
$x->bround(6);                  # must be no-op
ok ($x,'12345');

$x = $mbf->new('0.0061'); $x->bfround(-2); ok ($x,'0.01');
$x = $mbf->new('0.004'); $x->bfround(-2);  ok ($x,'0.00');
$x = $mbf->new('0.005'); $x->bfround(-2);  ok ($x,'0.00');

$x = $mbf->new('12345'); $x->bfround(2); ok ($x,'12340');
$x = $mbf->new('12340'); $x->bfround(2); ok ($x,'12340');

# MBI::bfround should clear A for negative P
$x = $mbi->new('1234'); $x->accuracy(3); $x->bfround(-2);
ok_undef ($x->{_a});

###############################################################################
# rounding with already set precision/accuracy

$x = $mbf->new(1); $x->{_p} = -5;
ok ($x,'1.00000');

# further rounding donw
ok ($x->bfround(-2),'1.00');
ok ($x->{_p},-2);

$x = $mbf->new(12345); $x->{_a} = 5;
ok ($x->bround(2),'12000');
ok ($x->{_a},2);

$x = $mbf->new('1.2345'); $x->{_a} = 5;
ok ($x->bround(2),'1.2');
ok ($x->{_a},2);

# mantissa/exponent format and A/P
$x = $mbf->new('12345.678'); $x->accuracy(4);
ok ($x,'12350'); ok ($x->{_a},4); ok_undef ($x->{_p});
ok_undef ($x->{_m}->{_a}); ok_undef ($x->{_e}->{_a});
ok_undef ($x->{_m}->{_p}); ok_undef ($x->{_e}->{_p});

# check for no A/P in case of fallback
# result
$x = $mbf->new(100) / 3;
ok_undef ($x->{_a}); ok_undef ($x->{_p});

# result & reminder
$x = $mbf->new(100) / 3; ($x,$y) = $x->bdiv(3);
ok_undef ($x->{_a}); ok_undef ($x->{_p});
ok_undef ($y->{_a}); ok_undef ($y->{_p});

###############################################################################
# math with two numbers with differen A and P

$x = $mbf->new(12345); $x->accuracy(4);		# '12340'
$y = $mbf->new(12345); $y->accuracy(2);		# '12000'
ok ($x+$y,24000);				# 12340+12000=> 24340 => 24000

$x = $mbf->new(54321); $x->accuracy(4);		# '12340'
$y = $mbf->new(12345); $y->accuracy(3);		# '12000'
ok ($x-$y,42000);				# 54320+12300=> 42020 => 42000

$x = $mbf->new('1.2345'); $x->precision(-2);	# '1.23'
$y = $mbf->new('1.2345'); $y->precision(-4);	# '1.2345'
ok ($x+$y,'2.46');				# 1.2345+1.2300=> 2.4645 => 2.46

###############################################################################
# round should find and use proper class

#$x = Foo->new();
#ok ($x->round($Foo::accuracy),'a' x $Foo::accuracy);
#ok ($x->round(undef,$Foo::precision),'p' x $Foo::precision);
#ok ($x->bfround($Foo::precision),'p' x $Foo::precision);
#ok ($x->bround($Foo::accuracy),'a' x $Foo::accuracy);

###############################################################################
# find out whether _find_round_parameters is doing what's it's supposed to do
 
${"$mbi\::accuracy"} = undef;
${"$mbi\::precision"} = undef;
${"$mbi\::div_scale"} = 40;
${"$mbi\::round_mode"} = 'odd';
 
$x = $mbi->new(123);
my @params = $x->_find_round_parameters();
ok (scalar @params,1);				# nothing to round

@params = $x->_find_round_parameters(1);
ok (scalar @params,4);				# a=1
ok ($params[0],$x);				# self
ok ($params[1],1);				# a
ok_undef ($params[2]);				# p
ok ($params[3],'odd');				# round_mode

@params = $x->_find_round_parameters(undef,2);
ok (scalar @params,4);				# p=2
ok ($params[0],$x);				# self
ok_undef ($params[1]);				# a
ok ($params[2],2);				# p
ok ($params[3],'odd');				# round_mode

eval { @params = $x->_find_round_parameters(undef,2,'foo'); };
ok ($@ =~ /^Unknown round mode 'foo'/,1);

@params = $x->_find_round_parameters(undef,2,'+inf');
ok (scalar @params,4);				# p=2
ok ($params[0],$x);				# self
ok_undef ($params[1]);				# a
ok ($params[2],2);				# p
ok ($params[3],'+inf');				# round_mode

@params = $x->_find_round_parameters(2,-2,'+inf');
ok (scalar @params,1);				# error, A and P defined
ok ($params[0],$x);				# self

${"$mbi\::accuracy"} = 1;
@params = $x->_find_round_parameters(undef,-2);
ok (scalar @params,1);				# error, A and P defined
ok ($params[0],$x);				# self

${"$mbi\::accuracy"} = undef;
${"$mbi\::precision"} = 1;
@params = $x->_find_round_parameters(1,undef);
ok (scalar @params,1);				# error, A and P defined
ok ($params[0],$x);				# self

${"$mbi\::precision"} = undef;			# reset

###############################################################################
# test whether bone/bzero take additional A & P, or reset it etc

foreach my $c ($mbi,$mbf)
  {
  $x = $c->new(2)->bzero(); ok_undef ($x->{_a}); ok_undef ($x->{_p});
  $x = $c->new(2)->bone();  ok_undef ($x->{_a}); ok_undef ($x->{_p});
  $x = $c->new(2)->binf();  ok_undef ($x->{_a}); ok_undef ($x->{_p});
  $x = $c->new(2)->bnan();  ok_undef ($x->{_a}); ok_undef ($x->{_p});

  $x = $c->new(2); $x->{_a} = 1; $x->{_p} = 2; $x->bnan();
  ok_undef ($x->{_a}); ok_undef ($x->{_p});
  $x = $c->new(2); $x->{_a} = 1; $x->{_p} = 2; $x->binf();
  ok_undef ($x->{_a}); ok_undef ($x->{_p});

  $x = $c->new(2,1); ok ($x->{_a},1); ok_undef ($x->{_p});
  $x = $c->new(2,undef,1); ok_undef ($x->{_a}); ok ($x->{_p},1);
  
  $x = $c->new(2,1)->bzero(); ok ($x->{_a},1); ok_undef ($x->{_p});
  $x = $c->new(2,undef,1)->bzero(); ok_undef ($x->{_a}); ok ($x->{_p},1);

  $x = $c->new(2,1)->bone(); ok ($x->{_a},1); ok_undef ($x->{_p});
  $x = $c->new(2,undef,1)->bone(); ok_undef ($x->{_a}); ok ($x->{_p},1);

  $x = $c->new(2); $x->bone('+',2,undef); ok ($x->{_a},2); ok_undef ($x->{_p});
  $x = $c->new(2); $x->bone('+',undef,2); ok_undef ($x->{_a}); ok ($x->{_p},2);
  $x = $c->new(2); $x->bone('-',2,undef); ok ($x->{_a},2); ok_undef ($x->{_p});
  $x = $c->new(2); $x->bone('-',undef,2); ok_undef ($x->{_a}); ok ($x->{_p},2);
  
  $x = $c->new(2); $x->bzero(2,undef); ok ($x->{_a},2); ok_undef ($x->{_p});
  $x = $c->new(2); $x->bzero(undef,2); ok_undef ($x->{_a}); ok ($x->{_p},2);
  }

###############################################################################
# test whether bone/bzero honour globals

for my $c ($mbi,$mbf)
  {
  $c->accuracy(2);
  $x = $c->bone(); ok ($x->accuracy(),2);
  $x = $c->bzero(); ok ($x->accuracy(),2);
  $c->accuracy(undef);
  
  $c->precision(-2);
  $x = $c->bone(); ok ($x->precision(),-2);
  $x = $c->bzero(); ok ($x->precision(),-2);
  $c->precision(undef);
  }

###############################################################################
# check whether mixing A and P creates a NaN

# new with set accuracy/precision and with parameters

foreach my $c ($mbi,$mbf)
  {
  ok ($c->new(123,4,-3),'NaN');			# with parameters
  ${"$c\::accuracy"} = 42;
  ${"$c\::precision"} = 2;
  ok ($c->new(123),'NaN');			# with globals
  ${"$c\::accuracy"} = undef;
  ${"$c\::precision"} = undef;
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
    print "# Tried: '$try'\n" if !ok ($rc, 'NaN');
    }
  }

# unary ops
foreach (qw/new bsqrt/)
  {
  my $try = 'my $x = $mbi->$_(1234,5,-3); ';
  $rc = eval $try;
  print "# Tried: '$try'\n" if !ok ($rc, 'NaN');
  }

# see if $x->bsub(0) and $x->badd(0) really round
foreach my $class ($mbi,$mbf)
  {
  $x = $class->new(123); $class->accuracy(2); $x->bsub(0);
  ok ($x,120);
  $class->accuracy(undef);
  $x = $class->new(123); $class->accuracy(2); $x->badd(0);
  ok ($x,120);
  $class->accuracy(undef);
  }

###############################################################################
# test whether shortcuts returning zero/one preserve A and P

my ($ans1,$f,$a,$p,$xp,$yp,$xa,$ya,$try,$ans,@args);
my $CALC = Math::BigInt->config()->{lib};
while (<DATA>)
  {
  chomp;
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
  
  #  print "trying $try\n";
  $rc = eval $try;
  # convert hex/binary targets to decimal
  if ($ans =~ /^(0x0x|0b0b)/)
    {
    $ans =~ s/^0[xb]//;
    $ans = $mbi->new($ans)->bstr();
    }
  print "# Tried: '$try'\n" if !ok ($rc, $ans);
  # check internal state of number objects
  is_valid($rc,$f) if ref $rc;

  # now check whether A and P are set correctly
  # only one of $a or $p will be set (no crossing here)
  $a = $xa || $ya; $p = $xp || $yp;

  # print "Check a=$a p=$p\n";
  # print "# Tried: '$try'\n";
  if ($a ne '')
    {
    if (!(ok ($x->{_a}, $a) && ok_undef ($x->{_p})))
      {
      print "# Check: A=$a and P=undef\n";
      print "# Tried: '$try'\n";
      } 
    }
  if ($p ne '')
    {
    if (!(ok ($x->{_p}, $p) && ok_undef ($x->{_a})))
      {
      print "# Check: A=undef and P=$p\n";
      print "# Tried: '$try'\n";
      }
    }
  }

# all done
1;

###############################################################################
###############################################################################
# Perl 5.005 does not like ok ($x,undef)

sub ok_undef
  {
  my $x = shift;

  ok (1,1) and return 1 if !defined $x;
  ok ($x,'undef');
  print "# Called from ",join(' ',caller()),"\n";
  return 0;
  }

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
  ok (1,1), return if ($e eq '0');

  ok (1,$e." after op '$f'");
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
