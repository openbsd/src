# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 1457;

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
        my $test = qq|\$x = $class -> from_hex("$in0");|;

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
                     . qq| \$x -> from_hex("$in0");|;

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

0x0:0
0x1:1
0x2:2
0x3:3
0x4:4
0x5:5
0x6:6
0x7:7
0x8:8
0x9:9
0xa:10
0xb:11
0xc:12
0xd:13
0xe:14
0xf:15
0x10:16
0x11:17

0xfe:254
0xff:255
0x100:256
0x101:257

0xfffe:65534
0xffff:65535
0x10000:65536
0x10001:65537

0xfffffe:16777214
0xffffff:16777215
0x1000000:16777216
0x1000001:16777217

0xfffffffe:4294967294
0xffffffff:4294967295
0x100000000:4294967296
0x100000001:4294967297

0xfffffffffe:1099511627774
0xffffffffff:1099511627775
0x10000000000:1099511627776
0x10000000001:1099511627777

0xfffffffffffe:281474976710654
0xffffffffffff:281474976710655
0x1000000000000:281474976710656
0x1000000000001:281474976710657

0xfffffffffffffe:72057594037927934
0xffffffffffffff:72057594037927935
0x100000000000000:72057594037927936
0x100000000000001:72057594037927937

0X10:16
x10:16
X10:16

NaN:NaN
+inf:NaN
-inf:NaN
