# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 16;

use Math::BigInt;

while (<DATA>) {
    s/#.*$//;                   # remove comments
    s/\s+$//;                   # remove trailing whitespace
    next unless length;         # skip empty lines

    my ($in0, $out0, $out1, $out2, $out3) = split /:/;
    my ($ss, $sa, $es, $ea);

    my $test = q|($ss, $sa, $es, $ea) = |
             . qq|Math::BigInt -> _dec_str_to_str_parts("$in0")|;

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
0e-0:+:0:+:0
0e-7:+:0:+:0
0e+7:+:0:+:0

0.0120:+:.012:+:0
0120.0:+:120:+:0
0120.0340:+:120.034:+:0

1.e0:+:1:+:0

00.0012003400E0056007800:+:.00120034:+:56007800

+1__2__.__3__4__e+5__6__:+:12.34:+:56
+1__2__.__3__4__e-5__6__:+:12.34:-:56
-1__2__.__3__4__e+5__6__:-:12.34:+:56
-1__2__.__3__4__e-5__6__:-:12.34:-:56

1__2__.__3__4__e5__6__:+:12.34:+:56
1__2__.__3__4__e-5__6__:+:12.34:-:56
-1__2__.__3__4__e5__6__:-:12.34:+:56
