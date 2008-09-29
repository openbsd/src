#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;

plan tests => 6;

my %h;

ok (!Internals::HvREHASH(%h), "hash doesn't start with rehash flag on");

foreach (1..10) {
  $h{"\0"x$_}++;
}

ok (!Internals::HvREHASH(%h), "10 entries doesn't trigger rehash");

foreach (11..20) {
  $h{"\0"x$_}++;
}

ok (Internals::HvREHASH(%h), "20 entries triggers rehash");




# second part using an emulation of the PERL_HASH in perl, mounting an
# attack on a pre-populated hash. This is also useful if you need normal
# keys which don't contain \0 -- suitable for stashes

use constant MASK_U32  => 2**32;
use constant HASH_SEED => 0;
use constant THRESHOLD => 14;
use constant START     => "a";

# some initial hash data
my %h2 = map {$_ => 1} 'a'..'cc';

ok (!Internals::HvREHASH(%h2), 
    "starting with pre-populated non-pathological hash (rehash flag if off)");

my @keys = get_keys(\%h2);
$h2{$_}++ for @keys;
ok (Internals::HvREHASH(%h2), 
    scalar(@keys) . " colliding into the same bucket keys are triggering rehash");

sub get_keys {
    my $hr = shift;

    # the minimum of bits required to mount the attack on a hash
    my $min_bits = log(THRESHOLD)/log(2);

    # if the hash has already been populated with a significant amount
    # of entries the number of mask bits can be higher
    my $keys = scalar keys %$hr;
    my $bits = $keys ? log($keys)/log(2) : 0;
    $bits = $min_bits if $min_bits > $bits;

    $bits = int($bits) < $bits ? int($bits) + 1 : int($bits);
    # need to add 2 bits to cover the internal split cases
    $bits += 2;
    my $mask = 2**$bits-1;
    print "# using mask: $mask ($bits)\n";

    my @keys;
    my $s = START;
    my $c = 0;
    # get 2 keys on top of the THRESHOLD
    my $hash;
    while (@keys < THRESHOLD+2) {
        # next if exists $hash->{$s};
        $hash = hash($s);
        next unless ($hash & $mask) == 0;
        $c++;
        printf "# %2d: %5s, %10s\n", $c, $s, $hash;
        push @keys, $s;
    } continue {
        $s++;
    }

    return @keys;
}


# trying to provide the fastest equivalent of C macro's PERL_HASH in
# Perl - the main complication is that it uses U32 integer, which we
# can't do it perl, without doing some tricks
sub hash {
    my $s = shift;
    my @c = split //, $s;
    my $u = HASH_SEED;
    for (@c) {
        # (A % M) + (B % M) == (A + B) % M
        # This works because '+' produces a NV, which is big enough to hold
        # the intermediate result. We only need the % before any "^" and "&"
        # to get the result in the range for an I32.
        # and << doesn't work on NV, so using 1 << 10
        $u += ord;
        $u += $u * (1 << 10); $u %= MASK_U32;
        $u ^= $u >> 6;
    }
    $u += $u << 3;  $u %= MASK_U32;
    $u ^= $u >> 11; $u %= MASK_U32;
    $u += $u << 15; $u %= MASK_U32;
    $u;
}

# This will crash perl if it fails

use constant PVBM => 'foo';

my $dummy = index 'foo', PVBM;
eval { my %h = (a => PVBM); 1 };

ok (!$@, 'fbm scalar can be inserted into a hash');
