#!/usr/bin/perl -w

###############################################################################

use Test;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 40;
  }

use bigrat qw/oct hex/;

###############################################################################
# general tests

my $x = 5; ok (ref($x) =~ /^Math::BigInt/);		# :constant

# todo:  ok (2 + 2.5,4.5);				# should still work
# todo: $x = 2 + 3.5; ok (ref($x),'Math::BigFloat');

$x = 2 ** 255; ok (ref($x) =~ /^Math::BigInt/);

# see if Math::BigRat constant works
ok (1/3, '1/3');
ok (1/4+1/3,'7/12');
ok (5/7+3/7,'8/7');

ok (3/7+1,'10/7');
ok (3/7+1.1,'107/70');
ok (3/7+3/7,'6/7');

ok (3/7-1,'-4/7');
ok (3/7-1.1,'-47/70');
ok (3/7-2/7,'1/7');

# fails ?
# ok (1+3/7,'10/7');

ok (1.1+3/7,'107/70');
ok (3/7*5/7,'15/49');
ok (3/7 / (5/7),'3/5');
ok (3/7 / 1,'3/7');
ok (3/7 / 1.5,'2/7');

###############################################################################
# accurarcy and precision

ok_undef (bigrat->accuracy());
ok (bigrat->accuracy(12),12);
ok (bigrat->accuracy(),12);

ok_undef (bigrat->precision());
ok (bigrat->precision(12),12);
ok (bigrat->precision(),12);

ok (bigrat->round_mode(),'even');
ok (bigrat->round_mode('odd'),'odd');
ok (bigrat->round_mode(),'odd');

###############################################################################
# hex() and oct()

my $c = 'Math::BigInt';

ok (ref(hex(1)), $c);
ok (ref(hex(0x1)), $c);
ok (ref(hex("af")), $c);
ok (hex("af"), Math::BigInt->new(0xaf));
ok (ref(hex("0x1")), $c);

ok (ref(oct("0x1")), $c);
ok (ref(oct("01")), $c);
ok (ref(oct("0b01")), $c);
ok (ref(oct("1")), $c);
ok (ref(oct(" 1")), $c);
ok (ref(oct(" 0x1")), $c);

ok (ref(oct(0x1)), $c);
ok (ref(oct(01)), $c);
ok (ref(oct(0b01)), $c);
ok (ref(oct(1)), $c);

###############################################################################
###############################################################################
# Perl 5.005 does not like ok ($x,undef)

sub ok_undef
  {
  my $x = shift;

  ok (1,1) and return if !defined $x;
  ok ($x,'undef');
  }
