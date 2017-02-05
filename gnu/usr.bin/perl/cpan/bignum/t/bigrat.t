#!perl

###############################################################################

use strict;
use warnings;

use Test::More tests => 40;

use bigrat qw/oct hex/;

###############################################################################
# general tests

my $x = 5;
like(ref($x), qr/^Math::BigInt/, '$x = 5 makes $x a Math::BigInt'); # :constant

# todo:  is(2 + 2.5, 4.5);				# should still work
# todo: $x = 2 + 3.5; is(ref($x), 'Math::BigFloat');

$x = 2 ** 255;
like(ref($x), qr/^Math::BigInt/, '$x = 2 ** 255 makes $x a Math::BigInt');

# see if Math::BigRat constant works
is(1/3,         '1/3',    qq|1/3 = '1/3'|);
is(1/4+1/3,     '7/12',   qq|1/4+1/3 = '7/12'|);
is(5/7+3/7,     '8/7',    qq|5/7+3/7 = '8/7'|);

is(3/7+1,       '10/7',   qq|3/7+1 = '10/7'|);
is(3/7+1.1,     '107/70', qq|3/7+1.1 = '107/70'|);
is(3/7+3/7,     '6/7',    qq|3/7+3/7 = '6/7'|);

is(3/7-1,       '-4/7',   qq|3/7-1 = '-4/7'|);
is(3/7-1.1,     '-47/70', qq|3/7-1.1 = '-47/70'|);
is(3/7-2/7,     '1/7',    qq|3/7-2/7 = '1/7'|);

# fails ?
# is(1+3/7, '10/7', qq|1+3/7 = '10/7'|);

is(1.1+3/7,     '107/70', qq|1.1+3/7 = '107/70'|);
is(3/7*5/7,     '15/49',  qq|3/7*5/7 = '15/49'|);
is(3/7 / (5/7), '3/5',    qq|3/7 / (5/7) = '3/5'|);
is(3/7 / 1,     '3/7',    qq|3/7 / 1 = '3/7'|);
is(3/7 / 1.5,   '2/7',    qq|3/7 / 1.5 = '2/7'|);

###############################################################################
# accuracy and precision

is(bigrat->accuracy(),        undef, 'get accuracy');
is(bigrat->accuracy(12),      12,    'set accuracy to 12');
is(bigrat->accuracy(),        12,    'get accuracy again');

is(bigrat->precision(),       undef, 'get precision');
is(bigrat->precision(12),     12,    'set precision to 12');
is(bigrat->precision(),       12,    'get precision again');

is(bigrat->round_mode(),      'even', 'get round mode');
is(bigrat->round_mode('odd'), 'odd',  'set round mode');
is(bigrat->round_mode(),      'odd',  'get round mode again');

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
