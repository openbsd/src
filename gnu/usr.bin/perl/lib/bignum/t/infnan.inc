
use strict;

my ($x);

###############################################################################
# inf tests

$x = 1+inf;	ok (ref($x) =~ /^Math::BigInt/); ok ($x->bstr(),'inf');
$x = 1*inf;	ok (ref($x) =~ /^Math::BigInt/); ok ($x->bstr(),'inf');

# these don't work without exporting inf()
$x = inf;	ok (ref($x) =~ /^Math::BigInt/); ok ($x->bstr(),'inf');
$x = inf+inf;	ok (ref($x) =~ /^Math::BigInt/); ok ($x->bstr(),'inf');
$x = inf*inf;	ok (ref($x) =~ /^Math::BigInt/); ok ($x->bstr(),'inf');

###############################################################################
# NaN tests

$x = 1+NaN;	ok (ref($x) =~ /^Math::BigInt/); ok ($x->bstr(),'NaN');
$x = 1*NaN;	ok (ref($x) =~ /^Math::BigInt/); ok ($x->bstr(),'NaN');

# these don't work without exporting NaN()
$x = NaN;	ok (ref($x) =~ /^Math::BigInt/); ok ($x->bstr(),'NaN');
$x = NaN+NaN;	ok (ref($x) =~ /^Math::BigInt/); ok ($x->bstr(),'NaN');
$x = NaN*NaN;	ok (ref($x) =~ /^Math::BigInt/); ok ($x->bstr(),'NaN');

###############################################################################
# mixed tests

# these don't work without exporting NaN() or inf()
$x = NaN+inf;	ok (ref($x) =~ /^Math::BigInt/); ok ($x->bstr(),'NaN');
$x = NaN*inf;	ok (ref($x) =~ /^Math::BigInt/); ok ($x->bstr(),'NaN');
$x = inf*NaN;	ok (ref($x) =~ /^Math::BigInt/); ok ($x->bstr(),'NaN');

