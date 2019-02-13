#!perl

use strict;
use warnings;

use Test::More tests => 1373;

my $class;

BEGIN { $class = 'Math::BigInt'; }
BEGIN { use_ok($class); }

my @data;
my $space = "\t\r\n ";

while (<DATA>) {
    s/#.*$//;           # remove comments
    s/\s+$//;           # remove trailing whitespace
    next unless length; # skip empty lines

    my ($in0, $out0) = split /:/;

    push @data, [ $in0, $out0 ],
                [ $in0 . $space, $out0 ],
                [ $space . $in0, $out0 ],
                [ $space . $in0 . $space, $out0 ];
}

for my $entry (@data) {
    my ($in0, $out0) = @$entry;

    # As class method.

    {
        my $x;
        my $test = qq|\$x = $class -> from_oct("$in0");|;

        eval $test;
        die $@ if $@;           # this should never happen

        subtest $test, sub {
            plan tests => 2,

            is(ref($x), $class, "output arg is a $class");
            is($x, $out0, 'output arg has the right value');
        };
    }

    # As instance method.

    {
        for my $str ("-1", "0", "1", "-inf", "+inf", "NaN") {
            my $x;
            my $test = qq|\$x = $class -> new("$str");|
                     . qq| \$x -> from_oct("$in0");|;

            eval $test;
            die $@ if $@;       # this should never happen

            subtest $test, sub {
                plan tests => 2,

                is(ref($x), $class, "output arg is a $class");
                is($x, $out0, 'output arg has the right value');
            };
        }
    }
}

__END__

0:0
1:1
2:2
3:3
4:4
5:5
6:6
7:7
10:8
11:9
12:10
13:11
14:12
15:13
16:14
17:15
20:16
21:17

376:254
377:255
400:256
401:257

177776:65534
177777:65535
200000:65536
200001:65537

77777776:16777214
77777777:16777215
100000000:16777216
100000001:16777217

37777777776:4294967294
37777777777:4294967295
40000000000:4294967296
40000000001:4294967297

17777777777776:1099511627774
17777777777777:1099511627775
20000000000000:1099511627776
20000000000001:1099511627777

7777777777777776:281474976710654
7777777777777777:281474976710655
10000000000000000:281474976710656
10000000000000001:281474976710657

3777777777777777776:72057594037927934
3777777777777777777:72057594037927935
4000000000000000000:72057594037927936
4000000000000000001:72057594037927937

NaN:NaN
+inf:NaN
-inf:NaN
