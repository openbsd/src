use strict;
use warnings;
use Test::More tests => 22;

use Math::BigRat;

my $x = Math::BigRat->new('3/7');

for my $op (qw(& | ^ << >> &= |= ^= <<= >>=)) {
    ok !eval "my \$y = \$x $op 42; 1";
    like $@, qr/^bitwise operation \Q$op\E not supported in Math::BigRat/;
}

ok !eval "my \$y = ~\$x; 1";
like $@, qr/^bitwise operation ~ not supported in Math::BigRat/;
