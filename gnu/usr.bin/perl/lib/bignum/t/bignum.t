#!/usr/bin/perl -w

###############################################################################

use Test;
use strict;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib';
  plan tests => 20;
  }

use bignum;

###############################################################################
# general tests

my $x = 5; ok (ref($x) =~ /^Math::BigInt/);		# :constant

ok (2 + 2.5,4.5);
$x = 2 + 3.5; ok (ref($x),'Math::BigFloat');
ok (2 * 2.1,4.2);
$x = 2 + 2.1; ok (ref($x),'Math::BigFloat');

$x = 2 ** 255; ok (ref($x) =~ /^Math::BigInt/);

# see if Math::BigInt constant and upgrading works
ok (Math::BigInt::bsqrt('12'),'3.464101615137754587054892683011744733886');
ok (sqrt(12),'3.464101615137754587054892683011744733886');

ok (2/3,"0.6666666666666666666666666666666666666667");

#ok (2 ** 0.5, 'NaN');	# should be sqrt(2);

ok (12->bfac(),479001600);

# see if Math::BigFloat constant works

#                     0123456789          0123456789	<- default 40
#           0123456789          0123456789
ok (1/3, '0.3333333333333333333333333333333333333333');

###############################################################################
# accurarcy and precision

ok_undef (bignum->accuracy());
ok (bignum->accuracy(12),12);
ok (bignum->accuracy(),12);

ok_undef (bignum->precision());
ok (bignum->precision(12),12);
ok (bignum->precision(),12);

ok (bignum->round_mode(),'even');
ok (bignum->round_mode('odd'),'odd');
ok (bignum->round_mode(),'odd');

###############################################################################
###############################################################################
# Perl 5.005 does not like ok ($x,undef)

sub ok_undef
  {
  my $x = shift;

  ok (1,1) and return if !defined $x;
  ok ($x,'undef');
  }
