#!perl

use strict;
use warnings;

use Test::More tests => 7;

use Math::BigInt ':constant';

is(2 ** 255,
   '578960446186580977117854925043439539266'
   . '34992332820282019728792003956564819968',
   '2 ** 255');

{
    no warnings 'portable';     # protect against "non-portable" warnings

    # hexadecimal constants
    is(0x123456789012345678901234567890,
       Math::BigInt->new('0x123456789012345678901234567890'),
       'hexadecimal constant 0x123456789012345678901234567890');

    # binary constants
    is(0b01010100011001010110110001110011010010010110000101101101,
       Math::BigInt->new('0b0101010001100101011011000111'
                         . '0011010010010110000101101101'),
       'binary constant 0b0101010001100101011011000111'
       . '0011010010010110000101101101');
}

use Math::BigFloat ':constant';
is(1.0 / 3.0, '0.3333333333333333333333333333333333333333',
   '1.0 / 3.0 = 0.3333333333333333333333333333333333333333');

# stress-test Math::BigFloat->import()

Math::BigFloat->import(qw/:constant/);
pass('Math::BigFloat->import(qw/:constant/);');

Math::BigFloat->import(qw/:constant upgrade Math::BigRat/);
pass('Math::BigFloat->import(qw/:constant upgrade Math::BigRat/);');

Math::BigFloat->import(qw/upgrade Math::BigRat :constant/);
pass('Math::BigFloat->import(qw/upgrade Math::BigRat :constant/);');

# all tests done
