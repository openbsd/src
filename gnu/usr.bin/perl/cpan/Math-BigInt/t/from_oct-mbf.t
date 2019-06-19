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

01p+0:1
0.4p+1:1
0.2p+2:1
0.1p+3:1
0.04p+4:1
02p-1:1
04p-2:1
010p-3:1

-1p+0:-1

0p+0:0
0p+7:0
0p-7:0
0.p+0:0
.0p+0:0
0.0p+0:0

145376:51966
0145376:51966
00145376:51966

3.1p+2:12.5
22.15p-1:9.1015625
-0.361152746757p+32:-2023406814.9375
44.3212636115p+30:39093746765

NaN:NaN
+inf:NaN
-inf:NaN
.p+0:NaN
