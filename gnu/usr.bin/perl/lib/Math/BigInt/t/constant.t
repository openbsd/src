#!/usr/bin/perl -w

use strict;
use Test;

BEGIN
  {
  $| = 1;
  chdir 't' if -d 't';
  unshift @INC, '../lib'; # for running manually
  plan tests => 7;
  if ($] < 5.006)
    {
    for (1..7) { skip (1,'Not supported on older Perls'); }
    exit;
    }
  } 

use Math::BigInt ':constant';

ok (2 ** 255,'57896044618658097711785492504343953926634992332820282019728792003956564819968');

{
  no warnings 'portable';	# protect against "non-portable" warnings
# hexadecimal constants
ok (0x123456789012345678901234567890,
    Math::BigInt->new('0x123456789012345678901234567890'));
# binary constants
ok (0b01010100011001010110110001110011010010010110000101101101,
    Math::BigInt->new(
     '0b01010100011001010110110001110011010010010110000101101101'));
}

use Math::BigFloat ':constant';
ok (1.0 / 3.0, '0.3333333333333333333333333333333333333333');

# stress-test Math::BigFloat->import()

Math::BigFloat->import( qw/:constant/ );
ok (1,1);

Math::BigFloat->import( qw/:constant upgrade Math::BigRat/ );
ok (1,1);

Math::BigFloat->import( qw/upgrade Math::BigRat :constant/ );
ok (1,1);

# all tests done

