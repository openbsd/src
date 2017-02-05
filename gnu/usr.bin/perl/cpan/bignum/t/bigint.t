#!perl

###############################################################################

use strict;
use warnings;

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
    my ($x, $y) = split /:/;
    is(bigint::_float_constant("$x"), "$y",
       qq|bigint::_float_constant("$x") = $y|);
}

foreach (qw/
  0100:64
  0200:128
  0x100:256
  0b1001:9
  /)
{
    my ($x, $y) = split /:/;
    is(bigint::_binary_constant("$x"), "$y",
       qq|bigint::_binary_constant("$x") = "$y")|);
}

###############################################################################
# general tests

my $x = 5;
like(ref($x), qr/^Math::BigInt/, '$x = 5 makes $x a Math::BigInt'); # :constant

# todo:  is(2 + 2.5, 4.5);                              # should still work
# todo: $x = 2 + 3.5; is(ref($x), 'Math::BigFloat');

$x = 2 ** 255;
like(ref($x), qr/^Math::BigInt/, '$x = 2 ** 255 makes $x a Math::BigInt');

is(12->bfac(), 479001600, '12->bfac() = 479001600');
is(9/4, 2, '9/4 = 2');

is(4.5 + 4.5, 8, '4.5 + 4.5 = 2');                         # truncate
like(ref(4.5 + 4.5), qr/^Math::BigInt/, '4.5 + 4.5 makes a Math::BigInt');

###############################################################################
# accuracy and precision

is(bigint->accuracy(),        undef, 'get accuracy');
is(bigint->accuracy(12),      12,    'set accuracy to 12');
is(bigint->accuracy(),        12,    'get accuracy again');

is(bigint->precision(),       undef, 'get precision');
is(bigint->precision(12),     12,    'set precision to 12');
is(bigint->precision(),       12,    'get precision again');

is(bigint->round_mode(),      'even', 'get round mode');
is(bigint->round_mode('odd'), 'odd',  'set round mode');
is(bigint->round_mode(),      'odd',  'get round mode again');

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
