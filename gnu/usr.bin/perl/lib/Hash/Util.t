#!/usr/bin/perl -Tw

BEGIN {
    if( $ENV{PERL_CORE} ) {
        @INC = '../lib';
        chdir 't';
    }
}
use Test::More tests => 179;
use strict;

my @Exported_Funcs;
BEGIN { 
    @Exported_Funcs = qw(lock_keys   unlock_keys
                         lock_value  unlock_value
                         lock_hash   unlock_hash
                         hash_seed
                        );
    use_ok 'Hash::Util', @Exported_Funcs;
}
foreach my $func (@Exported_Funcs) {
    can_ok __PACKAGE__, $func;
}

my %hash = (foo => 42, bar => 23, locked => 'yep');
lock_keys(%hash);
eval { $hash{baz} = 99; };
like( $@, qr/^Attempt to access disallowed key 'baz' in a restricted hash/,
                                                       'lock_keys()');
is( $hash{bar}, 23 );
ok( !exists $hash{baz} );

delete $hash{bar};
ok( !exists $hash{bar} );
$hash{bar} = 69;
is( $hash{bar}, 69 );

eval { () = $hash{i_dont_exist} };
like( $@, qr/^Attempt to access disallowed key 'i_dont_exist' in a restricted hash/ );

lock_value(%hash, 'locked');
eval { print "# oops" if $hash{four} };
like( $@, qr/^Attempt to access disallowed key 'four' in a restricted hash/ );

eval { $hash{"\x{2323}"} = 3 };
like( $@, qr/^Attempt to access disallowed key '(.*)' in a restricted hash/,
                                               'wide hex key' );

eval { delete $hash{locked} };
like( $@, qr/^Attempt to delete readonly key 'locked' from a restricted hash/,
                                           'trying to delete a locked key' );
eval { $hash{locked} = 42; };
like( $@, qr/^Modification of a read-only value attempted/,
                                           'trying to change a locked key' );
is( $hash{locked}, 'yep' );

eval { delete $hash{I_dont_exist} };
like( $@, qr/^Attempt to delete disallowed key 'I_dont_exist' from a restricted hash/,
                             'trying to delete a key that doesnt exist' );

ok( !exists $hash{I_dont_exist} );

unlock_keys(%hash);
$hash{I_dont_exist} = 42;
is( $hash{I_dont_exist}, 42,    'unlock_keys' );

eval { $hash{locked} = 42; };
like( $@, qr/^Modification of a read-only value attempted/,
                             '  individual key still readonly' );
eval { delete $hash{locked} },
is( $@, '', '  but can be deleted :(' );

unlock_value(%hash, 'locked');
$hash{locked} = 42;
is( $hash{locked}, 42,  'unlock_value' );


{
    my %hash = ( foo => 42, locked => 23 );

    lock_keys(%hash);
    eval { %hash = ( wubble => 42 ) };  # we know this will bomb
    like( $@, qr/^Attempt to access disallowed key 'wubble'/ );
    unlock_keys(%hash);
}

{ 
    my %hash = (KEY => 'val', RO => 'val');
    lock_keys(%hash);
    lock_value(%hash, 'RO');

    eval { %hash = (KEY => 1) };
    like( $@, qr/^Attempt to delete readonly key 'RO' from a restricted hash/ );
}

{
    my %hash = (KEY => 1, RO => 2);
    lock_keys(%hash);
    eval { %hash = (KEY => 1, RO => 2) };
    is( $@, '');
}



{
    my %hash = ();
    lock_keys(%hash, qw(foo bar));
    is( keys %hash, 0,  'lock_keys() w/keyset shouldnt add new keys' );
    $hash{foo} = 42;
    is( keys %hash, 1 );
    eval { $hash{wibble} = 42 };
    like( $@, qr/^Attempt to access disallowed key 'wibble' in a restricted hash/,
                        '  locked');

    unlock_keys(%hash);
    eval { $hash{wibble} = 23; };
    is( $@, '', 'unlock_keys' );
}


{
    my %hash = (foo => 42, bar => undef, baz => 0);
    lock_keys(%hash, qw(foo bar baz up down));
    is( keys %hash, 3,   'lock_keys() w/keyset didnt add new keys' );
    is_deeply( \%hash, { foo => 42, bar => undef, baz => 0 } );

    eval { $hash{up} = 42; };
    is( $@, '' );

    eval { $hash{wibble} = 23 };
    like( $@, qr/^Attempt to access disallowed key 'wibble' in a restricted hash/, '  locked' );
}


{
    my %hash = (foo => 42, bar => undef);
    eval { lock_keys(%hash, qw(foo baz)); };
    is( $@, sprintf("Hash has key 'bar' which is not in the new key ".
                    "set at %s line %d\n", __FILE__, __LINE__ - 2) );
}


{
    my %hash = (foo => 42, bar => 23);
    lock_hash( %hash );

    ok( Internals::SvREADONLY(%hash) );
    ok( Internals::SvREADONLY($hash{foo}) );
    ok( Internals::SvREADONLY($hash{bar}) );

    unlock_hash ( %hash );

    ok( !Internals::SvREADONLY(%hash) );
    ok( !Internals::SvREADONLY($hash{foo}) );
    ok( !Internals::SvREADONLY($hash{bar}) );
}


lock_keys(%ENV);
eval { () = $ENV{I_DONT_EXIST} };
like( $@, qr/^Attempt to access disallowed key 'I_DONT_EXIST' in a restricted hash/,   'locked %ENV');

{
    my %hash;

    lock_keys(%hash, 'first');

    is (scalar keys %hash, 0, "place holder isn't a key");
    $hash{first} = 1;
    is (scalar keys %hash, 1, "we now have a key");
    delete $hash{first};
    is (scalar keys %hash, 0, "now no key");

    unlock_keys(%hash);

    $hash{interregnum} = 1.5;
    is (scalar keys %hash, 1, "key again");
    delete $hash{interregnum};
    is (scalar keys %hash, 0, "no key again");

    lock_keys(%hash, 'second');

    is (scalar keys %hash, 0, "place holder isn't a key");

    eval {$hash{zeroeth} = 0};
    like ($@,
          qr/^Attempt to access disallowed key 'zeroeth' in a restricted hash/,
          'locked key never mentioned before should fail');
    eval {$hash{first} = -1};
    like ($@,
          qr/^Attempt to access disallowed key 'first' in a restricted hash/,
          'previously locked place holders should also fail');
    is (scalar keys %hash, 0, "and therefore there are no keys");
    $hash{second} = 1;
    is (scalar keys %hash, 1, "we now have just one key");
    delete $hash{second};
    is (scalar keys %hash, 0, "back to zero");

    unlock_keys(%hash); # We have deliberately left a placeholder.

    $hash{void} = undef;
    $hash{nowt} = undef;

    is (scalar keys %hash, 2, "two keys, values both undef");

    lock_keys(%hash);

    is (scalar keys %hash, 2, "still two keys after locking");

    eval {$hash{second} = -1};
    like ($@,
          qr/^Attempt to access disallowed key 'second' in a restricted hash/,
          'previously locked place holders should fail');

    is ($hash{void}, undef,
        "undef values should not be misunderstood as placeholders");
    is ($hash{nowt}, undef,
        "undef values should not be misunderstood as placeholders (again)");
}

{
  # perl #18651 - tim@consultix-inc.com found a rather nasty data dependant
  # bug whereby hash iterators could lose hash keys (and values, as the code
  # is common) for restricted hashes.

  my @keys = qw(small medium large);

  # There should be no difference whether it is restricted or not
  foreach my $lock (0, 1) {
    # Try setting all combinations of the 3 keys
    foreach my $usekeys (0..7) {
      my @usekeys;
      for my $bits (0,1,2) {
	push @usekeys, $keys[$bits] if $usekeys & (1 << $bits);
      }
      my %clean = map {$_ => length $_} @usekeys;
      my %target;
      lock_keys ( %target, @keys ) if $lock;

      while (my ($k, $v) = each %clean) {
	$target{$k} = $v;
      }

      my $message
	= ($lock ? 'locked' : 'not locked') . ' keys ' . join ',', @usekeys;

      is (scalar keys %target, scalar keys %clean, "scalar keys for $message");
      is (scalar values %target, scalar values %clean,
	  "scalar values for $message");
      # Yes. All these sorts are necessary. Even for "identical hashes"
      # Because the data dependency of the test involves two of the strings
      # colliding on the same bucket, so the iterator order (output of keys,
      # values, each) depends on the addition order in the hash. And locking
      # the keys of the hash involves behind the scenes key additions.
      is_deeply( [sort keys %target] , [sort keys %clean],
		 "list keys for $message");
      is_deeply( [sort values %target] , [sort values %clean],
		 "list values for $message");

      is_deeply( [sort %target] , [sort %clean],
		 "hash in list context for $message");

      my (@clean, @target);
      while (my ($k, $v) = each %clean) {
	push @clean, $k, $v;
      }
      while (my ($k, $v) = each %target) {
	push @target, $k, $v;
      }

      is_deeply( [sort @target] , [sort @clean],
		 "iterating with each for $message");
    }
  }
}

# Check clear works on locked empty hashes - SEGVs on 5.8.2.
{
    my %hash;
    lock_hash(%hash);
    %hash = ();
    ok(keys(%hash) == 0, 'clear empty lock_hash() hash');
}
{
    my %hash;
    lock_keys(%hash);
    %hash = ();
    ok(keys(%hash) == 0, 'clear empty lock_keys() hash');
}

my $hash_seed = hash_seed();
ok($hash_seed >= 0, "hash_seed $hash_seed");

{
    package Minder;
    my $counter;
    sub DESTROY {
	--$counter;
    }
    sub new {
	++$counter;
	bless [], __PACKAGE__;
    }
    package main;

    for my $state ('', 'locked') {
	my $a = Minder->new();
	is ($counter, 1, "There is 1 object $state");
	my %hash;
	$hash{a} = $a;
	is ($counter, 1, "There is still 1 object $state");

	lock_keys(%hash) if $state;

	is ($counter, 1, "There is still 1 object $state");
	undef $a;
	is ($counter, 1, "Still 1 object $state");
	delete $hash{a};
	is ($counter, 0, "0 objects when hash key is deleted $state");
	$hash{a} = undef;
	is ($counter, 0, "Still 0 objects $state");
	%hash = ();
	is ($counter, 0, "0 objects after clear $state");
    }
}

{
    my %hash = map {$_,$_} qw(fwiffffff foosht teeoo);
    lock_keys(%hash);
    delete $hash{fwiffffff};
    is (scalar keys %hash, 2);
    unlock_keys(%hash);
    is (scalar keys %hash, 2);

    my ($first, $value) = each %hash;
    is ($hash{$first}, $value, "Key has the expected value before the lock");
    lock_keys(%hash);
    is ($hash{$first}, $value, "Key has the expected value after the lock");

    my ($second, $v2) = each %hash;

    is ($hash{$first}, $value, "Still correct after iterator advances");
    is ($hash{$second}, $v2, "Other key has the expected value");
}
