#!/usr/bin/perl -w

###############################################################################

use Test::More tests => 51;

use bigint qw/hex oct/;

###############################################################################
# _constant tests

foreach (qw/ 
  123:123
  123.4:123
  1.4:1
  0.1:0
  -0.1:0
  -1.1:-1
  -123.4:-123
  -123:-123
  123e2:123e2
  123e-1:12
  123e-4:0
  123e-3:0
  123.345e-1:12
  123.456e+2:12345
  1234.567e+3:1234567
  1234.567e+4:1234567E1
  1234.567e+6:1234567E3
  /)
  {
  my ($x,$y) = split /:/;
  print "# Try $x\n";
  is (bigint::_float_constant("$x"),"$y");
  }

foreach (qw/ 
  0100:64
  0200:128
  0x100:256
  0b1001:9
  /)
  {
  my ($x,$y) = split /:/;
  print "# Try $x\n";
  is (bigint::_binary_constant("$x"),"$y");
  }

###############################################################################
# general tests

my $x = 5; like (ref($x), qr/^Math::BigInt/);		# :constant

# todo:  is (2 + 2.5,4.5);				# should still work
# todo: $x = 2 + 3.5; is (ref($x),'Math::BigFloat');

$x = 2 ** 255; like (ref($x), qr/^Math::BigInt/);

is (12->bfac(),479001600);
is (9/4,2);

is (4.5+4.5,8);					# truncate
like (ref(4.5+4.5), qr/^Math::BigInt/);


###############################################################################
# accuracy and precision

is ('bigint'->accuracy(), undef);
is ('bigint'->accuracy(12),12);
is ('bigint'->accuracy(),12);

is ('bigint'->precision(), undef);
is ('bigint'->precision(12),12);
is ('bigint'->precision(),12);

is ('bigint'->round_mode(),'even');
is ('bigint'->round_mode('odd'),'odd');
is ('bigint'->round_mode(),'odd');

###############################################################################
# hex() and oct()

my $c = 'Math::BigInt';

is (ref(hex(1)), $c);
is (ref(hex(0x1)), $c);
is (ref(hex("af")), $c);
is (hex("af"), Math::BigInt->new(0xaf));
is (ref(hex("0x1")), $c);

is (ref(oct("0x1")), $c);
is (ref(oct("01")), $c);
is (ref(oct("0b01")), $c);
is (ref(oct("1")), $c);
is (ref(oct(" 1")), $c);
is (ref(oct(" 0x1")), $c);

is (ref(oct(0x1)), $c);
is (ref(oct(01)), $c);
is (ref(oct(0b01)), $c);
is (ref(oct(1)), $c);
