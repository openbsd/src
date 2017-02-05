#!perl

###############################################################################

use strict;
use warnings;

use Test::More;

if (eval { require Math::BigInt::Lite; 1 }) {
    plan tests => 1;
    # can use Lite, so let bignum try it
    require bigrat;
    bigrat->import();
    # can't get to work a ref(1+1) here, presumable because :constant phase
    # already done
    is($bigrat::_lite, 1, '$bigrat::_lite is 1');
} else {
    plan skip_all => "no Math::BigInt::Lite";
}
