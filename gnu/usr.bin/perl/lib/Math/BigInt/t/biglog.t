#!/usr/bin/perl -w

# Test blog function (and bpow, since it uses blog).

# It is too slow to be simple included in bigfltpm.inc, where it would get
# executed 3 times. One time would be under BareCalc, which shouldn't make any
# difference since there is no CALC->_log() function, and one time under a
# subclass, which *should* work.

# But it is better to test the numerical functionality, instead of not testing
# it at all (which did lead to wrong answers for 0 < $x < 1 in blog() in
# versions up to v1.63, and for bsqrt($x) when $x << 1 for instance).

use Test;
use strict;

BEGIN
  {
  $| = 1;
  # to locate the testing files
  my $location = $0; $location =~ s/biglog.t//i;
  if ($ENV{PERL_CORE})
    {
    # testing with the core distribution
    @INC = qw(../lib);
    }
  unshift @INC, '../lib';
  if (-d 't')
    {
    chdir 't';
    require File::Spec;
    unshift @INC, File::Spec->catdir(File::Spec->updir, $location);
    }
  else
    {
    unshift @INC, $location;
    }
  print "# INC = @INC\n";

  plan tests => 53;
  }

use Math::BigFloat;
use Math::BigInt;

my $cl = "Math::BigFloat";

# These tests are now really fast, since they collapse to blog(10), basically
# Don't attempt to run them with older versions. You are warned.

# $x < 0 => NaN
ok ($cl->new(-2)->blog(), 'NaN');
ok ($cl->new(-1)->blog(), 'NaN');
ok ($cl->new(-10)->blog(), 'NaN');
ok ($cl->new(-2,2)->blog(), 'NaN');

my $ten = $cl->new(10)->blog();

# 10 is cached (up to 75 digits)
ok ($cl->new(10)->blog(), '2.302585092994045684017991454684364207601');

# 0.1 is using the cached value for log(10), too

ok ($cl->new(0.1)->blog(), -$ten);
ok ($cl->new(0.01)->blog(), -$ten * 2);
ok ($cl->new(0.001)->blog(), -$ten * 3);
ok ($cl->new(0.0001)->blog(), -$ten * 4);

# also cached
ok ($cl->new(2)->blog(), '0.6931471805599453094172321214581765680755');
ok ($cl->new(4)->blog(), $cl->new(2)->blog * 2);

# These are still slow, so do them only to 10 digits

ok ($cl->new('0.2')->blog(undef,10), '-1.609437912');
ok ($cl->new('0.3')->blog(undef,10), '-1.203972804');
ok ($cl->new('0.4')->blog(undef,10), '-0.9162907319');
ok ($cl->new('0.5')->blog(undef,10), '-0.6931471806');
ok ($cl->new('0.6')->blog(undef,10), '-0.5108256238');
ok ($cl->new('0.7')->blog(undef,10), '-0.3566749439');
ok ($cl->new('0.8')->blog(undef,10), '-0.2231435513');
ok ($cl->new('0.9')->blog(undef,10), '-0.1053605157');

ok ($cl->new('9')->blog(undef,10), '2.197224577');

ok ($cl->new('10')->blog(10,10),   '1.000000000');
ok ($cl->new('20')->blog(20,10),   '1.000000000');
ok ($cl->new('100')->blog(100,10), '1.000000000');

ok ($cl->new('100')->blog(10,10),  '2.000000000');	# 10 ** 2 == 100
ok ($cl->new('400')->blog(20,10),  '2.000000000');	# 20 ** 2 == 400

ok ($cl->new('4')->blog(2,10),  '2.000000000');		# 2 ** 2 == 4
ok ($cl->new('16')->blog(2,10), '4.000000000');		# 2 ** 4 == 16

ok ($cl->new('1.2')->bpow('0.3',10),  '1.056219968');	
ok ($cl->new('10')->bpow('0.6',10),   '3.981071706');

# blog should handle bigint input
ok (Math::BigFloat::blog(Math::BigInt->new(100),10), 2);

# some integer results
ok ($cl->new(2)->bpow(32)->blog(2),  '32');	# 2 ** 32
ok ($cl->new(3)->bpow(32)->blog(3),  '32');	# 3 ** 32
ok ($cl->new(2)->bpow(65)->blog(2),  '65');	# 2 ** 65

# test for bug in bsqrt() not taking negative _e into account
test_bpow ('200','0.5',10,      '14.14213562');
test_bpow ('20','0.5',10,       '4.472135955');
test_bpow ('2','0.5',10,        '1.414213562');
test_bpow ('0.2','0.5',10,      '0.4472135955');
test_bpow ('0.02','0.5',10,     '0.1414213562');
test_bpow ('0.49','0.5',undef , '0.7');
test_bpow ('0.49','0.5',10 ,    '0.7000000000');
test_bpow ('0.002','0.5',10,    '0.04472135955');
test_bpow ('0.0002','0.5',10,   '0.01414213562');
test_bpow ('0.0049','0.5',undef,'0.07');
test_bpow ('0.0049','0.5',10 ,  '0.07000000000');
test_bpow ('0.000002','0.5',10, '0.001414213562');
test_bpow ('0.021','0.5',10,    '0.1449137675');
test_bpow ('1.2','0.5',10,      '1.095445115');
test_bpow ('1.23','0.5',10,     '1.109053651');
test_bpow ('12.3','0.5',10,     '3.507135583');

test_bpow ('9.9','0.5',10,        '3.146426545');
test_bpow ('9.86902225','0.5',10, '3.141500000');
test_bpow ('9.86902225','0.5',undef, '3.1415');

test_bpow ('0.2','0.41',10,   '0.5169187652');

sub test_bpow
  {
  my ($x,$y,$scale,$result) = @_;

  print "# Tried: $x->bpow($y,$scale);\n"
   unless ok ($cl->new($x)->bpow($y,$scale),$result);
  }

