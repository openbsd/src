#!./perl -w
# Now they'll be wanting biff! and zap! tests too.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

# This calcualtion ought to be within 0.001 of the right answer.
my $bits_in_uv = int (0.001 + log (~0+1) / log 2);

# 3**30 < 2**48, don't trust things outside that range on a Cray
# Likewise other 3 should not overflow 48 bits if I did my sums right.
my @pow = ([3,30,1e-14], [4,32,0], [5,20,1e-14], [2.5, 10,,1e-14], [-2, 69,0]);
my $tests;
$tests += $_->[1] foreach @pow;

plan tests => 1 + $bits_in_uv + $tests;

# Ought to be 32, 64, 36 or something like that.

my $remainder = $bits_in_uv & 3;

cmp_ok ($remainder, '==', 0, 'Sanity check bits in UV calculation')
    or printf "# ~0 is %d (0x%d) which gives $bits_in_uv bits\n", ~0, ~0;

# These are a lot of brute force tests to see how accurate $m ** $n is.
# Unfortunately rather a lot of perl programs expect 2 ** $n to be integer
# perfect, forgetting that it's a call to floating point pow() which never
# claims to deliver perfection.
foreach my $n (0..$bits_in_uv - 1) {
    my $exp = 2 ** $n;
    my $int = 1 << $n;
    cmp_ok ($exp, '==', $int, "2 ** $n vs 1 << $n");
}

foreach my $pow (@pow) {
    my ($base, $max, $range) = @$pow;
    my $fp = 1;
    foreach my $n (0..$max-1) {
        my $exp = $base ** $n;
        within ($exp, $fp, $range, "$base ** $n [$exp] vs $base * $base * ...");
        $fp *= $base;
    }
}
