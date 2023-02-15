# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 36;

use Math::BigInt;

my $LIB = Math::BigInt -> config('lib');

sub try {
    my ($in0, $in1, $in2, $in3, $in4, $out0, $out1, $out2, $out3) = @_;

    my @out;
    my $test = q|@out = Math::BigInt -> _bin_parts_to_lib_parts|
             . qq|("$in0", "$in1", "$in2", "$in3", $in4)|;

    eval $test;
    die $@ if $@;       # this should never happen

    subtest $test => sub {
        plan tests => 5;

        is(scalar(@out), 4, 'number of output arguments');
        is($out[0], $out0, 'sign of the significand');
        is($LIB -> _str($out[1]), $out1, 'absolute value of the significand');
        is($out[2], $out2, 'sign of the exponent');
        is($LIB -> _str($out[3]), $out3, 'absolute value of the exponent');
    };
}

note("binary");

try qw< + 0                + 0    >, 1, qw< + 0     + 0 >;
try qw< + 00.000           - 0000 >, 1, qw< + 0     + 0 >;

try qw< + 1010             + 0    >, 1, qw< + 1     + 1 >;
try qw< + 1111             + 0    >, 1, qw< + 15    + 0 >;
try qw< + 0.1              + 0    >, 1, qw< + 5     - 1 >;

try qw< + 10               - 8    >, 1, qw< + 78125 - 7 >;
try qw< + 10               + 8    >, 1, qw< + 512   + 0 >;

try qw< + 11000000001100   - 0    >, 1, qw< + 123   + 2 >;
try qw< + 1100000000110000 - 2    >, 1, qw< + 123   + 2 >;

try qw< + .00110011        + 5    >, 1, qw< + 6375  - 3 >;

try qw< - 1100.0011        + 2    >, 1, qw< - 4875  - 2 >;

note("octal");

try qw< + 0         + 0     >, 3, qw< + 0     + 0 >;
try qw< + 00.000    - 0000  >, 3, qw< + 0     + 0 >;
try qw< + 12        + 0     >, 3, qw< + 1     + 1 >;
try qw< + 17        + 0     >, 3, qw< + 15    + 0 >;
try qw< + 0.4       + 0     >, 3, qw< + 5     - 1 >;
try qw< + 2         - 8     >, 3, qw< + 78125 - 7 >;
try qw< + 2         + 8     >, 3, qw< + 512   + 0 >;
try qw< + 30014     - 0     >, 3, qw< + 123   + 2 >;
try qw< + 14006     + 1     >, 3, qw< + 123   + 2 >;
try qw< + 12300     + 0     >, 3, qw< + 5312  + 0 >;

note("hexadecimal");

try qw< + 0         + 0     >, 4, qw<  + 0                       + 0  >;
try qw< + 00.000    - 0000  >, 4, qw<  + 0                       + 0  >;

try qw< + a         + 0     >, 4, qw<  + 1                       + 1  >;
try qw< + f         + 0     >, 4, qw<  + 15                      + 0  >;
try qw< + 0.8       + 0     >, 4, qw<  + 5                       - 1  >;

try qw< + 2         - 8     >, 4, qw<  + 78125                   - 7  >;
try qw< + 2         + 8     >, 4, qw<  + 512                     + 0  >;

try qw< + 300c      - 0     >, 4, qw<  + 123                     + 2  >;
try qw< + 1.806     + 13    >, 4, qw<  + 123                     + 2  >;
try qw< + c030      - 2     >, 4, qw<  + 123                     + 2  >;

try qw< + 0.0625    + 16    >, 4, qw<  + 1573                    + 0  >;

try qw< + .0123     + 0     >, 4, qw<  + 44403076171875          - 16 >;
try qw< + 12300     + 0     >, 4, qw<  + 74496                   + 0  >;

try qw< + .00120034 + 5     >, 4, qw<  + 87894499301910400390625 - 25 >;

try qw< - 1200.0034 + 2     >, 4, qw<  - 18432003173828125       - 12 >;
