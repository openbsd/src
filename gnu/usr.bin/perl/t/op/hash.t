#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;

plan tests => 15;

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
my %h2;
my $counter= "a";
$h2{$counter++}++ while $counter ne 'cd';

ok (!Internals::HvREHASH(%h2), 
    "starting with pre-populated non-pathological hash (rehash flag if off)");

my @keys = get_keys(\%h2);
my $buckets= buckets(\%h2);
$h2{$_}++ for @keys;
$h2{$counter++}++ while buckets(\%h2) == $buckets; # force a split
ok (Internals::HvREHASH(%h2), 
    scalar(@keys) . " colliding into the same bucket keys are triggering rehash after split");

# returns the number of buckets in a hash
sub buckets {
    my $hr = shift;
    my $keys_buckets= scalar(%$hr);
    if ($keys_buckets=~m!/([0-9]+)\z!) {
        return 0+$1;
    } else {
        return 8;
    }
}

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


my $destroyed;
{ package Class; DESTROY { ++$destroyed; } }

$destroyed = 0;
{
    my %h;
    keys(%h) = 1;
    $h{key} = bless({}, 'Class');
}
is($destroyed, 1, 'Timely hash destruction with lvalue keys');


# [perl #79178] Hash keys must not be stringified during compilation
# Run perl -MO=Concise -e '$a{\"foo"}' on a non-threaded pre-5.13.8 version
# to see why.
{
    my $key;
    package bar;
    sub TIEHASH { bless {}, $_[0] }
    sub FETCH { $key = $_[1] }
    package main;
    tie my %h, "bar";
    () = $h{\'foo'};
    is ref $key, SCALAR =>
     'hash keys are not stringified during compilation';
}

# Part of RT #85026: Deleting the current iterator in void context does not
# free it.
{
    my $gone;
    no warnings 'once';
    local *::DESTROY = sub { ++$gone };
    my %a=(a=>bless[]);
    each %a;   # make the entry with the obj the current iterator
    delete $a{a};
    ok $gone, 'deleting the current iterator in void context frees the val'
}

# [perl #99660] Deleted hash element visible to destructor
{
    my %h;
    $h{k} = bless [];
    my $normal_exit;
    local *::DESTROY = sub { my $x = $h{k}; ++$normal_exit };
    delete $h{k}; # must be in void context to trigger the bug
    ok $normal_exit, 'freed hash elems are not visible to DESTROY';
}

# [perl #100340] Similar bug: freeing a hash elem during a delete
sub guard::DESTROY {
   ${$_[0]}->();
};
*guard = sub (&) {
   my $callback = shift;
   return bless \$callback, "guard"
};
{
  my $ok;
  my %t; %t = (
    stash => {
        guard => guard(sub{
            $ok++;
            delete $t{stash};
        }),
        foo => "bar",
        bar => "baz",
    },
  );
  ok eval { delete $t{stash}{guard}; # must be in void context
            1 },
    'freeing a hash elem from destructor called by delete does not die';
  diag $@ if $@; # panic: free from wrong pool
  is $ok, 1, 'the destructor was called';
}

# Weak references to pad hashes
SKIP: {
    skip_if_miniperl("No Scalar::Util::weaken under miniperl", 1);
    my $ref;
    require Scalar::Util;
    {
        my %hash;
        Scalar::Util::weaken($ref = \%hash);
        1;  # the previous statement must not be the last
    }
    is $ref, undef, 'weak refs to pad hashes go stale on scope exit';
}

# [perl #107440]
sub A::DESTROY { $::ra = 0 }
$::ra = {a=>bless [], 'A'};
undef %$::ra;
pass 'no crash when freeing hash that is being undeffed';
$::ra = {a=>bless [], 'A'};
%$::ra = ('a'..'z');
pass 'no crash when freeing hash that is being exonerated, ahem, cleared';
