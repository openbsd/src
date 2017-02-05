#!perl

###############################################################################

use strict;
use warnings;

use Test::More;

if (eval { require Math::BigInt::Lite; 1 }) {
    plan tests => 1;
    # can use Lite, so let bignum try it
    require bignum;
    bignum->import();
    # can't get to work a ref(1+1) here, presumable because :constant phase
    # already done
    is($bignum::_lite, 1, '$bignum::_lite is 1');
} else {
    plan skip_all => "no Math::BigInt::Lite";
}
