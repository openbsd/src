#!perl

use strict;
use warnings;

use Test::More tests => 69;

my $class;

BEGIN { $class = 'Math::BigFloat'; }
BEGIN { use_ok($class, '1.999710'); }

while (<DATA>) {
    s/#.*$//;           # remove comments
    s/\s+$//;           # remove trailing whitespace
    next unless length; # skip empty lines

    my ($in0, $out0) = split /:/;
    my $x;

    my $test = qq|\$x = $class -> new("$in0");|;
    my $desc = $test;

    eval $test;
    die $@ if $@;       # this should never happen

    subtest $desc, sub {
        plan tests => 2,

        # Check output.

        is(ref($x), $class, "output arg is a $class");
        is($x, $out0, 'output arg has the right value');
    };

}

__END__

NaN:NaN
inf:inf
infinity:inf
+inf:inf
+infinity:inf
-inf:-inf
-infinity:-inf

# This is the same data as in from_hex-mbf.t, except that some of them are
# commented out, since new() only treats input as hexadecimal if it has a "0x"
# or "0X" prefix, possibly with a leading "+" or "-" sign.

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
#xcafe:51966
#cafe:51966

0x1.9p+3:12.5
0x12.34p-1:9.1015625
-0x.789abcdefp+32:-2023406814.9375
0x12.3456789ap+31:39093746765

#NaN:NaN
#+inf:NaN
#-inf:NaN
0x.p+0:NaN

# This is more or less the same data as in from_oct-mbf.t, except that some of
# them are commented out, since new() only treats input as octal if it has a
# "0" prefix and a binary exponent, and possibly a leading "+" or "-" sign.
# Duplicates from above are also commented out.

01p+0:1
00.4p+1:1
00.2p+2:1
00.1p+3:1
00.04p+4:1
02p-1:1
04p-2:1
010p-3:1

-01p+0:-1

00p+0:0
00p+7:0
00p-7:0
00.p+0:0
00.0p+0:0
#00.0p+0:0

#145376:51966
#0145376:51966
#00145376:51966

03.1p+2:12.5
022.15p-1:9.1015625
-00.361152746757p+32:-2023406814.9375
044.3212636115p+30:39093746765

#NaN:NaN
#+inf:NaN
#-inf:NaN
0.p+0:NaN

# This is the same data as in from_bin-mbf.t, except that some of them are
# commented out, since new() only treats input as binary if it has a "0b" or
# "0B" prefix, possibly with a leading "+" or "-" sign. Duplicates from above
# are also commented out.

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
#b1100101011111110:51966
#1100101011111110:51966

0b1.1001p+3:12.5
0b10010.001101p-1:9.1015625
-0b.11110001001101010111100110111101111p+31:-2023406814.9375
0b10.0100011010001010110011110001001101p+34:39093746765

#NaN:NaN
#+inf:NaN
#-inf:NaN
0b.p+0:NaN
