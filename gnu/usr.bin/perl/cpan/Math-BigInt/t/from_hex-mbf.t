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

0x1p+0:1
0x.8p+1:1
0x.4p+2:1
0x.2p+3:1
0x.1p+4:1
0x2p-1:1
0x4p-2:1
0x8p-3:1

-0x1p+0:-1

0x0p+0:0
0x0p+7:0
0x0p-7:0
0x0.p+0:0
0x.0p+0:0
0x0.0p+0:0

0xcafe:51966
xcafe:51966
cafe:51966

0x1.9p+3:12.5
0x12.34p-1:9.1015625
-0x.789abcdefp+32:-2023406814.9375
0x12.3456789ap+31:39093746765

NaN:NaN
+inf:NaN
-inf:NaN
0x.p+0:NaN
