#!/usr/bin/perl -w

###############################################################################

use strict;
use Test::More tests => 35;

use bignum qw/oct hex/;

###############################################################################
# general tests

my $x = 5; like (ref($x), qr/^Math::BigInt/);		# :constant

is (2 + 2.5,4.5);
$x = 2 + 3.5; is (ref($x),'Math::BigFloat');
is (2 * 2.1,4.2);
$x = 2 + 2.1; is (ref($x),'Math::BigFloat');

$x = 2 ** 255; like (ref($x), qr/^Math::BigInt/);

# see if Math::BigInt constant and upgrading works
is (Math::BigInt::bsqrt('12'),'3.464101615137754587054892683011744733886');
is (sqrt(12),'3.464101615137754587054892683011744733886');

is (2/3,"0.6666666666666666666666666666666666666667");

#is (2 ** 0.5, 'NaN');	# should be sqrt(2);

is (12->bfac(),479001600);

# see if Math::BigFloat constant works

#                     0123456789          0123456789	<- default 40
#           0123456789          0123456789
is (1/3, '0.3333333333333333333333333333333333333333');

###############################################################################
# accuracy and precision

is (bignum->accuracy(), undef);
is (bignum->accuracy(12),12);
is (bignum->accuracy(),12);

is (bignum->precision(), undef);
is (bignum->precision(12),12);
is (bignum->precision(),12);

is (bignum->round_mode(),'even');
is (bignum->round_mode('odd'),'odd');
is (bignum->round_mode(),'odd');

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
