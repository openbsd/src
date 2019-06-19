#!perl

use strict;
use warnings;

use Test::More tests => 729;

my $class;

BEGIN { $class = 'Math::BigFloat'; }
BEGIN { use_ok($class, '1.999710'); }

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
        my $test = qq|\$x = $class -> from_bin("$in0");|;

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
                     . qq| \$x -> from_bin("$in0");|;

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

0b1p+0:1
0b.1p+1:1
0b.01p+2:1
0b.001p+3:1
0b.0001p+4:1
0b10p-1:1
0b100p-2:1
0b1000p-3:1

-0b1p+0:-1

0b0p+0:0
0b0p+7:0
0b0p-7:0
0b0.p+0:0
0b.0p+0:0
0b0.0p+0:0

0b1100101011111110:51966
b1100101011111110:51966
1100101011111110:51966

0b1.1001p+3:12.5
0b10010.001101p-1:9.1015625
-0b.11110001001101010111100110111101111p+31:-2023406814.9375
0b10.0100011010001010110011110001001101p+34:39093746765

NaN:NaN
+inf:NaN
-inf:NaN
0b.p+0:NaN
