#!perl

use strict;
use warnings;

use Test::More tests => 22;

use Math::BigRat;

my $x = Math::BigRat->new('3/7');

for my $op (qw(& | ^ << >> &= |= ^= <<= >>=)) {
    my $test = "\$y = \$x $op 42";
    ok(!eval "my \$y = \$x $op 42; 1", $test);
    like($@, qr/^bitwise operation \Q$op\E not supported in Math::BigRat/,
         $test);
}

my $test = "\$y = ~\$x";
ok(!eval "my \$y = ~\$x; 1", $test);
like($@, qr/^bitwise operation ~ not supported in Math::BigRat/, $test);
