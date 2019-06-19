#!perl

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
         qq| (\$m, \$e) = \$x -> nparts();\n\n|);

    {
        my $x = Math::BigInt -> new($x_str);
        my ($mant_got, $expo_got) = $x -> nparts();

        isa_ok($mant_got, "Math::BigInt");
        isa_ok($expo_got, "Math::BigInt");

        is($mant_got, $mant_str, "value of mantissa");
        is($expo_got, $expo_str, "value of exponent");
        is($x,        $x_str,    "input is unmodified");
    }

    note(qq|\n\$x = Math::BigInt -> new("$x_str");|,
         qq| \$m = \$x -> nparts();\n\n|);

    {
        my $x = Math::BigInt -> new($x_str);
        my $mant_got = $x -> nparts();

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
10:1:1
100:1:2
1000:1:3
10000:1:4
100000:1:5
1000000:1:6
10000000:1:7
100000000:1:8
1000000000:1:9
10000000000:1:10
100000000000:1:11
1000000000000:1:12

12:NaN:1
120:NaN:2
1200:NaN:3
12000:NaN:4
120000:NaN:5
1200000:NaN:6
12000000:NaN:7
120000000:NaN:8
1200000000:NaN:9
12000000000:NaN:10
120000000000:NaN:11
1200000000000:NaN:12

123:NaN:2
1230:NaN:3
12300:NaN:4
123000:NaN:5
1230000:NaN:6
12300000:NaN:7
123000000:NaN:8
1230000000:NaN:9
12300000000:NaN:10
123000000000:NaN:11
1230000000000:NaN:12

1234:NaN:3
12340:NaN:4
123400:NaN:5
1234000:NaN:6
12340000:NaN:7
123400000:NaN:8
1234000000:NaN:9
12340000000:NaN:10
123400000000:NaN:11
1234000000000:NaN:12

3141592:NaN:6

# negativ: numbers

-1:-1:0
-10:-1:1
-100:-1:2
-1000:-1:3
-10000:-1:4
-100000:-1:5
-1000000:-1:6
-10000000:-1:7
-100000000:-1:8
-1000000000:-1:9
-10000000000:-1:10
-100000000000:-1:11
-1000000000000:-1:12

-12:NaN:1
-120:NaN:2
-1200:NaN:3
-12000:NaN:4
-120000:NaN:5
-1200000:NaN:6
-12000000:NaN:7
-120000000:NaN:8
-1200000000:NaN:9
-12000000000:NaN:10
-120000000000:NaN:11
-1200000000000:NaN:12

-123:NaN:2
-1230:NaN:3
-12300:NaN:4
-123000:NaN:5
-1230000:NaN:6
-12300000:NaN:7
-123000000:NaN:8
-1230000000:NaN:9
-12300000000:NaN:10
-123000000000:NaN:11
-1230000000000:NaN:12

-1234:NaN:3
-12340:NaN:4
-123400:NaN:5
-1234000:NaN:6
-12340000:NaN:7
-123400000:NaN:8
-1234000000:NaN:9
-12340000000:NaN:10
-123400000000:NaN:11
-1234000000000:NaN:12

-3141592:NaN:6
