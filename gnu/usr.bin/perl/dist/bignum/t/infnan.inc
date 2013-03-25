
use strict;

my ($x);

###############################################################################
# inf tests

$x = 1+inf;	like (ref($x), qr/^Math::BigInt/); is ($x->bstr(),'inf');
$x = 1*inf;	like (ref($x), qr/^Math::BigInt/); is ($x->bstr(),'inf');

# these don't work without exporting inf()
$x = inf;	like (ref($x), qr/^Math::BigInt/); is ($x->bstr(),'inf');
$x = inf+inf;	like (ref($x), qr/^Math::BigInt/); is ($x->bstr(),'inf');
$x = inf*inf;	like (ref($x), qr/^Math::BigInt/); is ($x->bstr(),'inf');

###############################################################################
# NaN tests

$x = 1+NaN;	like (ref($x), qr/^Math::BigInt/); is ($x->bstr(),'NaN');
$x = 1*NaN;	like (ref($x), qr/^Math::BigInt/); is ($x->bstr(),'NaN');

# these don't work without exporting NaN()
$x = NaN;	like (ref($x), qr/^Math::BigInt/); is ($x->bstr(),'NaN');
$x = NaN+NaN;	like (ref($x), qr/^Math::BigInt/); is ($x->bstr(),'NaN');
$x = NaN*NaN;	like (ref($x), qr/^Math::BigInt/); is ($x->bstr(),'NaN');

###############################################################################
# mixed tests

# these don't work without exporting NaN() or inf()
$x = NaN+inf;	like (ref($x), qr/^Math::BigInt/); is ($x->bstr(),'NaN');
$x = NaN*inf;	like (ref($x), qr/^Math::BigInt/); is ($x->bstr(),'NaN');
$x = inf*NaN;	like (ref($x), qr/^Math::BigInt/); is ($x->bstr(),'NaN');

