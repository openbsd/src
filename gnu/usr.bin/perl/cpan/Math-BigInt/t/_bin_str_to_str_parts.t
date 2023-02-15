# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 18;

use Math::BigInt;

while (<DATA>) {
    s/#.*$//;                   # remove comments
    s/\s+$//;                   # remove trailing whitespace
    next unless length;         # skip empty lines

    my ($in0, $out0, $out1, $out2, $out3) = split /:/;
    my ($ss, $sa, $es, $ea);

    my $test = q|($ss, $sa, $es, $ea) = |
             . qq|Math::BigInt -> _bin_str_to_str_parts("$in0")|;

    eval $test;
    die $@ if $@;       # this should never happen

    subtest $test => sub {
        plan tests => 4;
        is($ss, $out0, 'sign of the significand');
        is($sa, $out1, 'absolute value of the significand');
        is($es, $out2, 'sign of the exponent');
        is($ea, $out3, 'absolute value of the exponent');
    };
}

__DATA__

0:+:0:+:0
0p-0:+:0:+:0
0p-7:+:0:+:0
0p+7:+:0:+:0

0.0110:+:.011:+:0
0110.0:+:110:+:0
0110.0110:+:110.011:+:0

0b1.p0:+:1:+:0

00.0011001100P0056007800:+:.00110011:+:56007800

+1__1__.__1__1__p+5__6__:+:11.11:+:56
+1__1__.__1__1__p-5__6__:+:11.11:-:56
-1__1__.__1__1__p+5__6__:-:11.11:+:56
-1__1__.__1__1__p-5__6__:-:11.11:-:56

1__1__.__1__1__p5__6__:+:11.11:+:56
1__1__.__1__1__p-5__6__:+:11.11:-:56
-1__1__.__1__1__p5__6__:-:11.11:+:56

-0b__1__1__.__1__1__p-1__1__:-:11.11:-:11
-0B__1__1__.__1__1__P-1__1__:-:11.11:-:11
