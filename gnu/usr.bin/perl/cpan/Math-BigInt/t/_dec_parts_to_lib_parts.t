# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 30;

use Math::BigInt;

my $LIB = Math::BigInt -> config('lib');

sub try {
    my ($in0, $in1, $in2, $in3, $out0, $out1, $out2, $out3) = @_;

    my @out;
    my $test = q|@out = Math::BigInt -> _dec_parts_to_lib_parts|
             . qq|("$in0", "$in1", "$in2", "$in3")|;

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

try qw< + 0      + 0     >, qw< + 0 + 0 >;
try qw< + 00.000 - 0000  >, qw< + 0 + 0 >;

try qw<    + 0.01230 + 5 >, qw< + 123 + 1 >;
try qw<    + 0.1230  + 5 >, qw< + 123 + 2 >;
try qw<    + 1.230   + 5 >, qw< + 123 + 3 >;
try qw<   + 12.30    + 5 >, qw< + 123 + 4 >;
try qw<  + 123.0     + 5 >, qw< + 123 + 5 >;
try qw< + 1230.0     + 5 >, qw< + 123 + 6 >;

try qw<    + 0.01230 + 2 >, qw< + 123 - 2 >;
try qw<    + 0.1230  + 2 >, qw< + 123 - 1 >;
try qw<    + 1.230   + 2 >, qw< + 123 + 0 >;
try qw<   + 12.30    + 2 >, qw< + 123 + 1 >;
try qw<  + 123.0     + 2 >, qw< + 123 + 2 >;
try qw< + 1230.0     + 2 >, qw< + 123 + 3 >;

try qw<    + 0.01230 - 2 >, qw< + 123 - 6 >;
try qw<    + 0.1230  - 2 >, qw< + 123 - 5 >;
try qw<    + 1.230   - 2 >, qw< + 123 - 4 >;
try qw<   + 12.30    - 2 >, qw< + 123 - 3 >;
try qw<  + 123.0     - 2 >, qw< + 123 - 2 >;
try qw< + 1230.0     - 2 >, qw< + 123 - 1 >;

try qw<    + 0.01230 - 4 >, qw< + 123 - 8 >;
try qw<    + 0.1230  - 4 >, qw< + 123 - 7 >;
try qw<    + 1.230   - 4 >, qw< + 123 - 6 >;
try qw<   + 12.30    - 4 >, qw< + 123 - 5 >;
try qw<  + 123.0     - 4 >, qw< + 123 - 4 >;
try qw< + 1230.0     - 4 >, qw< + 123 - 3 >;

try qw< + .0123      + 0 >, qw< + 123 - 4 >;
try qw< + 12300      + 0 >, qw< + 123 + 2 >;

try qw< + .00120034  + 5 >, qw< + 120034 - 3 >;

try qw< - 1200.0034  + 2 >, qw< - 12000034 - 2 >;
