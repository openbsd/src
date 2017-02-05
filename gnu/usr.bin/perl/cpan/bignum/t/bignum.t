#!/usr/bin/perl -w

###############################################################################

use strict;
use Test::More tests => 35;

use bignum qw/oct hex/;

###############################################################################
# general tests

my $x = 5;
like(ref($x), qr/^Math::BigInt/, '$x = 5 makes $x a Math::BigInt'); # :constant

is(2 + 2.5, 4.5, '2 + 2.5 = 4.5');
$x = 2 + 3.5;
is(ref($x), 'Math::BigFloat', '$x = 2 + 3.5 makes $x a Math::BigFloat');

is(2 * 2.1, 4.2, '2 * 2.1 = 4.2');
$x = 2 + 2.1;
is(ref($x), 'Math::BigFloat', '$x = 2 + 2.1 makes $x a Math::BigFloat');

$x = 2 ** 255;
like(ref($x), qr/^Math::BigInt/, '$x = 2 ** 255 makes $x a Math::BigInt');

# see if Math::BigInt constant and upgrading works
is(Math::BigInt::bsqrt("12"), '3.464101615137754587054892683011744733886',
   'Math::BigInt::bsqrt("12")');
is(sqrt(12), '3.464101615137754587054892683011744733886',
   'sqrt(12)');

is(2/3, "0.6666666666666666666666666666666666666667", '2/3');

#is(2 ** 0.5, 'NaN');   # should be sqrt(2);

is(12->bfac(), 479001600, '12->bfac() = 479001600');

# see if Math::BigFloat constant works

#                     0123456789          0123456789    <- default 40
#           0123456789          0123456789
is(1/3, '0.3333333333333333333333333333333333333333', '1/3');

###############################################################################
# accuracy and precision

is(bignum->accuracy(),        undef,  'get accuracy');
is(bignum->accuracy(12),      12,     'set accuracy to 12');
is(bignum->accuracy(),        12,     'get accuracy again');

is(bignum->precision(),       undef,  'get precision');
is(bignum->precision(12),     12,     'set precision to 12');
is(bignum->precision(),       12,     'get precision again');

is(bignum->round_mode(),      'even', 'get round mode');
is(bignum->round_mode('odd'), 'odd',  'set round mode');
is(bignum->round_mode(),      'odd',  'get round mode again');

###############################################################################
# hex() and oct()

my $class = 'Math::BigInt';

is(ref(hex(1)),      $class, qq|ref(hex(1)) = $class|);
is(ref(hex(0x1)),    $class, qq|ref(hex(0x1)) = $class|);
is(ref(hex("af")),   $class, qq|ref(hex("af")) = $class|);
is(ref(hex("0x1")),  $class, qq|ref(hex("0x1")) = $class|);

is(hex("af"), Math::BigInt->new(0xaf),
   qq|hex("af") = Math::BigInt->new(0xaf)|);

is(ref(oct("0x1")),  $class, qq|ref(oct("0x1")) = $class|);
is(ref(oct("01")),   $class, qq|ref(oct("01")) = $class|);
is(ref(oct("0b01")), $class, qq|ref(oct("0b01")) = $class|);
is(ref(oct("1")),    $class, qq|ref(oct("1")) = $class|);
is(ref(oct(" 1")),   $class, qq|ref(oct(" 1")) = $class|);
is(ref(oct(" 0x1")), $class, qq|ref(oct(" 0x1")) = $class|);

is(ref(oct(0x1)),    $class, qq|ref(oct(0x1)) = $class|);
is(ref(oct(01)),     $class, qq|ref(oct(01)) = $class|);
is(ref(oct(0b01)),   $class, qq|ref(oct(0b01)) = $class|);
is(ref(oct(1)),      $class, qq|ref(oct(1)) = $class|);
