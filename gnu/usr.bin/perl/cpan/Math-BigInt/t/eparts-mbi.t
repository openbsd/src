# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 784;

use Math::BigInt;

while (<DATA>) {
    s/#.*$//;                   # remove comments
    s/\s+$//;                   # remove trailing whitespace
    next unless length;         # skip empty lines

    my ($x_str, $mant_str, $expo_str) = split /:/;

    note(qq|\n\$x = Math::BigInt -> new("$x_str");|,
         qq| (\$m, \$e) = \$x -> eparts();\n\n|);

    {
        my $x = Math::BigInt -> new($x_str);
        my ($mant_got, $expo_got) = $x -> eparts();

        isa_ok($mant_got, "Math::BigInt");
        isa_ok($expo_got, "Math::BigInt");

        is($mant_got, $mant_str, "value of mantissa");
        is($expo_got, $expo_str, "value of exponent");
        is($x,        $x_str,    "input is unmodified");
    }

    note(qq|\n\$x = Math::BigInt -> new("$x_str");|,
         qq| \$m = \$x -> eparts();\n\n|);

    {
        my $x = Math::BigInt -> new($x_str);
        my $mant_got = $x -> eparts();

        isa_ok($mant_got, "Math::BigInt");

        is($mant_got, $mant_str, "value of mantissa");
        is($x,        $x_str,    "input is unmodified");
    }

}

__DATA__

NaN:NaN:NaN

inf:inf:inf
-inf:-inf:inf

0:0:0

# positive numbers

1:1:0
10:10:0
100:100:0
1000:1:3
10000:10:3
100000:100:3
1000000:1:6
10000000:10:6
100000000:100:6
1000000000:1:9
10000000000:10:9
100000000000:100:9
1000000000000:1:12

12:12:0
120:120:0
1200:NaN:3
12000:12:3
120000:120:3
1200000:NaN:6
12000000:12:6
120000000:120:6
1200000000:NaN:9
12000000000:12:9
120000000000:120:9
1200000000000:NaN:12

123:123:0
1230:NaN:3
12300:NaN:3
123000:123:3
1230000:NaN:6
12300000:NaN:6
123000000:123:6
1230000000:NaN:9
12300000000:NaN:9
123000000000:123:9
1230000000000:NaN:12

1234:NaN:3
12340:NaN:3
123400:NaN:3
1234000:NaN:6
12340000:NaN:6
123400000:NaN:6
1234000000:NaN:9
12340000000:NaN:9
123400000000:NaN:9
1234000000000:NaN:12

3141592:NaN:6

# negativ: numbers

-1:-1:0
-10:-10:0
-100:-100:0
-1000:-1:3
-10000:-10:3
-100000:-100:3
-1000000:-1:6
-10000000:-10:6
-100000000:-100:6
-1000000000:-1:9
-10000000000:-10:9
-100000000000:-100:9
-1000000000000:-1:12

-12:-12:0
-120:-120:0
-1200:NaN:3
-12000:-12:3
-120000:-120:3
-1200000:NaN:6
-12000000:-12:6
-120000000:-120:6
-1200000000:NaN:9
-12000000000:-12:9
-120000000000:-120:9
-1200000000000:NaN:12

-123:-123:0
-1230:NaN:3
-12300:NaN:3
-123000:-123:3
-1230000:NaN:6
-12300000:NaN:6
-123000000:-123:6
-1230000000:NaN:9
-12300000000:NaN:9
-123000000000:-123:9
-1230000000000:NaN:12

-1234:NaN:3
-12340:NaN:3
-123400:NaN:3
-1234000:NaN:6
-12340000:NaN:6
-123400000:NaN:6
-1234000000:NaN:9
-12340000000:NaN:9
-123400000000:NaN:9
-1234000000000:NaN:12

-3141592:NaN:6
