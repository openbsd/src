#!/usr/bin/perl -w

use strict;
use Test::More tests => 7;

use Math::BigInt ':constant';

is (2 ** 255,'57896044618658097711785492504343953926634992332820282019728792003956564819968');

{
  no warnings 'portable';	# protect against "non-portable" warnings
# hexadecimal constants
is (0x123456789012345678901234567890,
    Math::BigInt->new('0x123456789012345678901234567890'));
# binary constants
is (0b01010100011001010110110001110011010010010110000101101101,
    Math::BigInt->new(
     '0b01010100011001010110110001110011010010010110000101101101'));
}

use Math::BigFloat ':constant';
is (1.0 / 3.0, '0.3333333333333333333333333333333333333333');

# stress-test Math::BigFloat->import()

Math::BigFloat->import( qw/:constant/ );
is (1,1);

Math::BigFloat->import( qw/:constant upgrade Math::BigRat/ );
is (1,1);

Math::BigFloat->import( qw/upgrade Math::BigRat :constant/ );
is (1,1);

# all tests done
