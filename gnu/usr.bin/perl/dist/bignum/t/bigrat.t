#!/usr/bin/perl -w

###############################################################################

use strict;
use Test::More tests => 40;

use bigrat qw/oct hex/;

###############################################################################
# general tests

my $x = 5; like (ref($x), qr/^Math::BigInt/);		# :constant

# todo:  is (2 + 2.5,4.5);				# should still work
# todo: $x = 2 + 3.5; is (ref($x),'Math::BigFloat');

$x = 2 ** 255; like (ref($x), qr/^Math::BigInt/);

# see if Math::BigRat constant works
is (1/3, '1/3');
is (1/4+1/3,'7/12');
is (5/7+3/7,'8/7');

is (3/7+1,'10/7');
is (3/7+1.1,'107/70');
is (3/7+3/7,'6/7');

is (3/7-1,'-4/7');
is (3/7-1.1,'-47/70');
is (3/7-2/7,'1/7');

# fails ?
# is (1+3/7,'10/7');

is (1.1+3/7,'107/70');
is (3/7*5/7,'15/49');
is (3/7 / (5/7),'3/5');
is (3/7 / 1,'3/7');
is (3/7 / 1.5,'2/7');

###############################################################################
# accuracy and precision

is (bigrat->accuracy(), undef);
is (bigrat->accuracy(12),12);
is (bigrat->accuracy(),12);

is (bigrat->precision(), undef);
is (bigrat->precision(12),12);
is (bigrat->precision(),12);

is (bigrat->round_mode(),'even');
is (bigrat->round_mode('odd'),'odd');
is (bigrat->round_mode(),'odd');

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
