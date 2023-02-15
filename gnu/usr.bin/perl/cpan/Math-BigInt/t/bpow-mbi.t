# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 99;

use Math::BigInt;

my $class = "Math::BigInt";

use Math::Complex ();

my $inf = $Math::Complex::Inf;
my $nan = $inf - $inf;

# The following is used to compute the data at the end of this file.

if (0) {
    for my $x (-$inf, -64, -3, -2, -1, 0, 1, 2, 3, 64, $inf) {
        for my $y (-$inf, -3, -2, -1, 0, 1, 2, 3, $inf) {

            # The exceptions here are based on Wolfram Alpha,
            # https://www.wolframalpha.com/

            my $z = $x == -$inf && $y == 0     ? $nan
                  : $x ==  $inf && $y == 0     ? $nan
                  : $x == -1    && $y == -$inf ? $nan
                  : $x == -1    && $y ==  $inf ? $nan
                  :                              int($x ** $y);

            # Unfortunately, Math::Big* uses "inf", not "Inf" as Perl.

            printf "%s\n", join ":", map {   $_ ==  $inf ?  "inf"
                                           : $_ == -$inf ? "-inf"
                                           : $_                   } $x, $y, $z;
        }
    }

    exit;
}

while (<DATA>) {
    s/#.*$//;                   # remove comments
    s/\s+$//;                   # remove trailing whitespace
    next unless length;         # skip empty lines

    my @args = split /:/;
    my $want = pop @args;

    my ($x, $y, $z);

    my $test = qq|\$x = $class -> new("$args[0]"); |
             . qq|\$y = $class -> new("$args[1]"); |
             . qq|\$z = \$x -> bpow(\$y)|;

    eval "$test";
    die $@ if $@;

    subtest $test => sub {
        plan tests => 5;

        is(ref($x), $class, "\$x is still a $class");

        is(ref($y), $class, "\$y is still a $class");
        is($y, $args[1], "\$y is unmodified");

        is(ref($z), $class, "\$z is a $class");
        is($z, $want, "\$z has the right value");
    };
}

__DATA__
-inf:-inf:0
-inf:-3:0
-inf:-2:0
-inf:-1:0
-inf:0:NaN
-inf:1:-inf
-inf:2:inf
-inf:3:-inf
-inf:inf:inf
-64:-inf:0
-64:-3:0
-64:-2:0
-64:-1:0
-64:0:1
-64:1:-64
-64:2:4096
-64:3:-262144
-64:inf:inf
-3:-inf:0
-3:-3:0
-3:-2:0
-3:-1:0
-3:0:1
-3:1:-3
-3:2:9
-3:3:-27
-3:inf:inf
-2:-inf:0
-2:-3:0
-2:-2:0
-2:-1:0
-2:0:1
-2:1:-2
-2:2:4
-2:3:-8
-2:inf:inf
-1:-inf:NaN
-1:-3:-1
-1:-2:1
-1:-1:-1
-1:0:1
-1:1:-1
-1:2:1
-1:3:-1
-1:inf:NaN
0:-inf:inf
0:-3:inf
0:-2:inf
0:-1:inf
0:0:1
0:1:0
0:2:0
0:3:0
0:inf:0
1:-inf:1
1:-3:1
1:-2:1
1:-1:1
1:0:1
1:1:1
1:2:1
1:3:1
1:inf:1
2:-inf:0
2:-3:0
2:-2:0
2:-1:0
2:0:1
2:1:2
2:2:4
2:3:8
2:inf:inf
3:-inf:0
3:-3:0
3:-2:0
3:-1:0
3:0:1
3:1:3
3:2:9
3:3:27
3:inf:inf
64:-inf:0
64:-3:0
64:-2:0
64:-1:0
64:0:1
64:1:64
64:2:4096
64:3:262144
64:inf:inf
inf:-inf:0
inf:-3:0
inf:-2:0
inf:-1:0
inf:0:NaN
inf:1:inf
inf:2:inf
inf:3:inf
inf:inf:inf
