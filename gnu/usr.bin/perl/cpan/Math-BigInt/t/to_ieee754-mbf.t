# -*- mode: perl; -*-

use strict;
use warnings;

use Test::More tests => 66;

use Math::BigFloat;

my @k = (16, 32, 64, 128);

sub stringify {
    my $x = shift;
    return "$x" unless $x -> is_finite();
    my $nstr = $x -> bnstr();
    my $sstr = $x -> bsstr();
    return length($nstr) < length($sstr) ? $nstr : $sstr;
}

for my $k (@k) {

    # Parameters specific to this format:

    my $b = 2;
    my $p = $k == 16 ? 11
          : $k == 32 ? 24
          : $k == 64 ? 53
          : $k - sprintf("%.0f", 4 * log($k)/log(2)) + 13;

    $b = Math::BigFloat -> new($b);
    $k = Math::BigFloat -> new($k);
    $p = Math::BigFloat -> new($p);
    my $w = $k - $p;

    my $emax = 2 ** ($w - 1) - 1;
    my $emin = 1 - $emax;

    my $format = 'binary' . $k;

    note("\nComputing test data for k = $k ...\n\n");

    my $binv = Math::BigFloat -> new("0.5");

    my $data =
      [

       {
        dsc => "smallest positive subnormal number",
        bin => "0"
             . ("0" x $w)
             . ("0" x ($p - 2)) . "1",
        asc => "$b ** ($emin) * $b ** (" . (1 - $p) . ") "
             . "= $b ** (" . ($emin + 1 - $p) . ")",
        mbf => $binv ** ($p - 1 - $emin),
       },

       {
        dsc => "largest subnormal number",
        bin => "0"
             . ("0" x $w)
             . ("1" x ($p - 1)),
        asc => "$b ** ($emin) * (1 - $b ** (" . (1 - $p) . "))",
        mbf => $binv ** (-$emin) * (1 - $binv ** ($p - 1)),
       },

       {
        dsc => "smallest positive normal number",
        bin => "0"
             . ("0" x ($w - 1)) . "1"
             . ("0" x ($p - 1)),
        asc => "$b ** ($emin)",
        mbf => $binv ** (-$emin),
       },

       {
        dsc => "largest normal number",
        bin => "0"
             . ("1" x ($w - 1)) . "0"
             . "1" x ($p - 1),
        asc => "$b ** $emax * ($b - $b ** (" . (1 - $p) . "))",
        mbf => $b ** $emax * ($b - $binv ** ($p - 1)),
       },

       {
        dsc => "largest number less than one",
        bin => "0"
             . "0" . ("1" x ($w - 2)) . "0"
             . "1" x ($p - 1),
        asc => "1 - $b ** (-$p)",
        mbf => 1 - $binv ** $p,
       },

       {
        dsc => "smallest number larger than one",
        bin => "0"
             . "0" . ("1" x ($w - 1))
             . ("0" x ($p - 2)) . "1",
        asc => "1 + $b ** (" . (1 - $p) . ")",
        mbf => 1 + $binv ** ($p - 1),
       },

       {
        dsc => "second smallest number larger than one",
        bin => "0"
             . "0" . ("1" x ($w - 1))
             . ("0" x ($p - 3)) . "10",
        asc => "1 + $b ** (" . (2 - $p) . ")",
        mbf => 1 + $binv ** ($p - 2),
       },

       {
        dsc => "one",
        bin => "0"
             . "0" . ("1" x ($w - 1))
             . "0" x ($p - 1),
        asc => "1",
        mbf => Math::BigFloat -> new("1"),
       },

       {
        dsc => "minus one",
        bin => "1"
             . "0" . ("1" x ($w - 1))
             . "0" x ($p - 1),
        asc => "-1",
        mbf => Math::BigFloat -> new("-1"),
       },

       {
        dsc => "two",
        bin => "0"
             . "1" . ("0" x ($w - 1))
             . ("0" x ($p - 1)),
        asc => "2",
        mbf => Math::BigFloat -> new("2"),
       },

       {
        dsc => "minus two",
        bin => "1"
             . "1" . ("0" x ($w - 1))
             . ("0" x ($p - 1)),
        asc => "-2",
        mbf => Math::BigFloat -> new("-2"),
       },

       {
        dsc => "positive zero",
        bin => "0"
             . ("0" x $w)
             . ("0" x ($p - 1)),
        asc => "+0",
        mbf => Math::BigFloat -> new("0"),
       },

       {
        dsc => "positive infinity",
        bin => "0"
             . ("1" x $w)
             . ("0" x ($p - 1)),
        asc => "+inf",
        mbf => Math::BigFloat -> new("inf"),
       },

       {
        dsc => "negative infinity",
        bin =>  "1"
             . ("1" x $w)
             . ("0" x ($p - 1)),
        asc => "-inf",
        mbf => Math::BigFloat -> new("-inf"),
       },

       {
        dsc => "NaN (encoding used by Perl on Cygwin)",
        bin => "1"
             . ("1" x $w)
             . ("1" . ("0" x ($p - 2))),
        asc => "NaN",
        mbf => Math::BigFloat -> new("NaN"),
       },

      ];

    for my $entry (@$data) {
        my $bin   = $entry -> {bin};
        my $bytes = pack "B*", $bin;
        my $hex   = unpack "H*", $bytes;

        note("\n", $entry -> {dsc}, " (k = $k): ", $entry -> {asc}, "\n\n");

        my $x = $entry -> {mbf};

        my $test = qq|Math::BigFloat -> new("| . stringify($x)
                 . qq|") -> to_ieee754("$format")|;

        my $got_bytes = $x -> to_ieee754($format);
        my $got_hex = unpack "H*", $got_bytes;
        $got_hex =~ s/(..)/\\x$1/g;

        my $expected_hex = $hex;
        $expected_hex =~ s/(..)/\\x$1/g;

        is($got_hex, $expected_hex);
    }
}

# These tests verify fixing CPAN RT #139960.

# binary16

{
    # largest subnormal number
    my $lo = Math::BigFloat -> from_ieee754("03ff", "binary16");

    # smallest normal number
    my $hi = Math::BigFloat -> from_ieee754("0400", "binary16");

    # compute an average weighted towards the larger of the two
    my $x = 0.25 * $lo + 0.75 * $hi;

    my $got = unpack "H*", $x -> to_ieee754("binary16");
    is($got, "0400",
       "6.102025508880615234375e-5 -> 0x0400");
}

{
    # largest number smaller than one
    my $lo = Math::BigFloat -> from_ieee754("3bff", "binary16");

    # one
    my $hi = Math::BigFloat -> from_ieee754("3c00", "binary16");

    # compute an average weighted towards the larger of the two
    my $x = 0.25 * $lo + 0.75 * $hi;

    my $got = unpack "H*", $x -> to_ieee754("binary16");
    is($got, "3c00", "9.998779296875e-1 -> 0x3c00");
}

# binary32

{
    # largest subnormal number
    my $lo = Math::BigFloat -> from_ieee754("007fffff", "binary32");

    # smallest normal number
    my $hi = Math::BigFloat -> from_ieee754("00800000", "binary32");

    # compute an average weighted towards the larger of the two
    my $x = 0.25 * $lo + 0.75 * $hi;

    my $got = unpack "H*", $x -> to_ieee754("binary32");
    is($got, "00800000",
       "1.1754943157898258998483097641290060955707622747...e-38 -> 0x00800000");
}

{
    # largest number smaller than one
    my $lo = Math::BigFloat -> from_ieee754("3f7fffff", "binary32");

    # one
    my $hi = Math::BigFloat -> from_ieee754("3f800000", "binary32");

    # compute an average weighted towards the larger of the two
    my $x = 0.25 * $lo + 0.75 * $hi;

    my $got = unpack "H*", $x -> to_ieee754("binary32");
    is($got, "3f800000",
       "9.9999998509883880615234375e-1 -> 0x3f800000");
}

# binary64

{
    # largest subnormal number
    my $lo = Math::BigFloat -> from_ieee754("000fffffffffffff", "binary64");

    # smallest normal number
    my $hi = Math::BigFloat -> from_ieee754("0010000000000000", "binary64");

    # compute an average weighted towards the larger of the two
    my $x = 0.25 * $lo + 0.75 * $hi;

    my $got = unpack "H*", $x -> to_ieee754("binary64");
    is($got, "0010000000000000",
       "2.2250738585072012595738212570207680200...e-308 -> 0x0010000000000000");
}

{
    # largest number smaller than one
    my $lo = Math::BigFloat -> from_ieee754("3fefffffffffffff", "binary64");

    # one
    my $hi = Math::BigFloat -> from_ieee754("3ff0000000000000", "binary64");

    # compute an average weighted towards the larger of the two
    my $x = 0.25 * $lo + 0.75 * $hi;

    my $got = unpack "H*", $x -> to_ieee754("binary64");
    is($got, "3ff0000000000000",
       "9.999999999999999722444243843710864894092...e-1 -> 0x3ff0000000000000");
}
