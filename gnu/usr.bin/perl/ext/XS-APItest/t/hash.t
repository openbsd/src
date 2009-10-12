#!perl -w

BEGIN {
  chdir 't' if -d 't';
  @INC = '../lib';
  push @INC, "::lib:$MacPerl::Architecture:" if $^O eq 'MacOS';
  require Config; import Config;
  if ($Config{'extensions'} !~ /\bXS\/APItest\b/) {
    # Look, I'm using this fully-qualified variable more than once!
    my $arch = $MacPerl::Architecture;
    print "1..0 # Skip: XS::APItest was not built\n";
    exit 0;
  }
}

use strict;
use utf8;
use Tie::Hash;
use Test::More 'no_plan';

BEGIN {use_ok('XS::APItest')};

sub preform_test;
sub test_present;
sub test_absent;
sub test_delete_present;
sub test_delete_absent;
sub brute_force_exists;
sub test_store;
sub test_fetch_present;
sub test_fetch_absent;

my $utf8_for_258 = chr 258;
utf8::encode $utf8_for_258;

my @testkeys = ('N', chr 198, chr 256);
my @keys = (@testkeys, $utf8_for_258);

foreach (@keys) {
  utf8::downgrade $_, 1;
}
main_tests (\@keys, \@testkeys, '');

foreach (@keys) {
  utf8::upgrade $_;
}
main_tests (\@keys, \@testkeys, ' [utf8 hash]');

{
  my %h = (a=>'cheat');
  tie %h, 'Tie::StdHash';
  # is bug 36327 fixed?
  my $result = ($] > 5.009) ? undef : 1;

  is (XS::APItest::Hash::store(\%h, chr 258,  1), $result);
    
  ok (!exists $h{$utf8_for_258},
      "hv_store doesn't insert a key with the raw utf8 on a tied hash");
}

if ($] > 5.009) {
    my $strtab = strtab();
    is (ref $strtab, 'HASH', "The shared string table quacks like a hash");
    my $wibble = "\0";
    eval {
	$strtab->{$wibble}++;
    };
    my $prefix = "Cannot modify shared string table in hv_";
    my $what = $prefix . 'fetch';
    like ($@, qr/^$what/,$what);
    eval {
	XS::APItest::Hash::store($strtab, 'Boom!',  1)
    };
    $what = $prefix . 'store';
    like ($@, qr/^$what/, $what);
    if (0) {
	A::B->method();
    }
    # DESTROY should be in there.
    eval {
	delete $strtab->{DESTROY};
    };
    $what = $prefix . 'delete';
    like ($@, qr/^$what/, $what);
    # I can't work out how to get to the code that flips the wasutf8 flag on
    # the hash key without some ikcy XS
}

{
    is_deeply([&XS::APItest::Hash::test_hv_free_ent], [2,2,1,1],
	      "hv_free_ent frees the value immediately");
    is_deeply([&XS::APItest::Hash::test_hv_delayfree_ent], [2,2,2,1],
	      "hv_delayfree_ent keeps the value around until FREETMPS");
}

foreach my $in ("", "N", "a\0b") {
    my $got = XS::APItest::Hash::test_share_unshare_pvn($in);
    is ($got, $in, "test_share_unshare_pvn");
}

if ($] > 5.009) {
    foreach ([\&XS::APItest::Hash::rot13_hash, \&rot13, "rot 13"],
	     [\&XS::APItest::Hash::bitflip_hash, \&bitflip, "bitflip"],
	    ) {
	my ($setup, $mapping, $name) = @$_;
	my %hash;
	my %placebo = (a => 1, p => 2, i => 4, e => 8);
	$setup->(\%hash);
	$hash{a}++; @hash{qw(p i e)} = (2, 4, 8);

	test_U_hash(\%hash, \%placebo, [f => 9, g => 10, h => 11], $mapping,
		    $name);
    }
    foreach my $upgrade_o (0, 1) {
	foreach my $upgrade_n (0, 1) {
	    my (%hash, %placebo);
	    XS::APItest::Hash::bitflip_hash(\%hash);
	    foreach my $new (["7", 65, 67, 80],
			     ["8", 163, 171, 215],
			     ["U", 2603, 2604, 2604],
			    ) {
		foreach my $code (78, 240, 256, 1336) {
		    my $key = chr $code;
		    # This is the UTF-8 byte sequence for the key.
		    my $key_utf8 = $key;
		    utf8::encode($key_utf8);
		    if ($upgrade_o) {
			$key .= chr 256;
			chop $key;
		    }
		    $hash{$key} = $placebo{$key} = $code;
		    $hash{$key_utf8} = $placebo{$key_utf8} = "$code as UTF-8";
		}
		my $name = 'bitflip ' . shift @$new;
		my @new_kv;
		foreach my $code (@$new) {
		    my $key = chr $code;
		    if ($upgrade_n) {
			$key .= chr 256;
			chop $key;
		    }
		    push @new_kv, $key, $_;
		}

		$name .= ' upgraded(orig) ' if $upgrade_o;
		$name .= ' upgraded(new) ' if $upgrade_n;
		test_U_hash(\%hash, \%placebo, \@new_kv, \&bitflip, $name);
	    }
	}
    }
}

sub test_precomputed_hashes {
    my $what = shift;
    my $hash_it = shift;
    my $ord = shift;
    my $key_copy = $_[0];
    $key_copy .= '';

    my %hash;
    is (XS::APItest::Hash::common({hv => \%hash,
				   "key$what" => $_[0],
				   val => $ord,
				   "hash_$what" => $hash_it,
				   action => XS::APItest::HV_FETCH_ISSTORE}),
	$ord, "store $ord with $what \$hash_it = $hash_it");
    is_deeply ([each %hash], [$_[0], $ord], "First key read is good");
    is_deeply ([each %hash], [], "No second key good");
    
    is ($hash{$_[0]}, $ord, "Direct hash read finds $ord");

    is_deeply ([each %hash], [$key_copy, $ord],
	       "First key read is good with a copy");
    is_deeply ([each %hash], [], "No second key good");
    
    is ($hash{$key_copy}, $ord, "Direct hash read finds $ord");
}

{
    my $as_utf8 = "\241" . chr 256;
    chop $as_utf8;
    my $as_bytes = "\243";
    foreach my $key ('N', $as_bytes, $as_utf8, "\x{2623}") {
	my $ord = ord $key;
	foreach my $hash_it (0, 1) {
	    foreach my $what (qw(pv sv)) {
		test_precomputed_hashes($what, $hash_it, $ord, $key);
	    }
	    # Generate a shared hash key scalar
	    my %h = ($key => 1);
	    test_precomputed_hashes('sv', $hash_it, $ord, (keys %h)[0]);
	}
    }
}

exit;

################################   The End   ################################

sub test_U_hash {
    my ($hash, $placebo, $new, $mapping, $message) = @_;
    my @hitlist = keys %$placebo;
    print "# $message\n";

    my @keys = sort keys %$hash;
    is ("@keys", join(' ', sort($mapping->(keys %$placebo))),
	"uvar magic called exactly once on store");

    is (keys %$hash, keys %$placebo);

    my $victim = shift @hitlist;
    is (delete $hash->{$victim}, delete $placebo->{$victim});

    is (keys %$hash, keys %$placebo);
    @keys = sort keys %$hash;
    is ("@keys", join(' ', sort($mapping->(keys %$placebo))));

    $victim = shift @hitlist;
    is (XS::APItest::Hash::delete_ent ($hash, $victim,
				       XS::APItest::HV_DISABLE_UVAR_XKEY),
	undef, "Deleting a known key with conversion disabled fails (ent)");
    is (keys %$hash, keys %$placebo);

    is (XS::APItest::Hash::delete_ent ($hash, $victim, 0),
	delete $placebo->{$victim},
	"Deleting a known key with conversion enabled works (ent)");
    is (keys %$hash, keys %$placebo);
    @keys = sort keys %$hash;
    is ("@keys", join(' ', sort($mapping->(keys %$placebo))));

    $victim = shift @hitlist;
    is (XS::APItest::Hash::delete ($hash, $victim,
				   XS::APItest::HV_DISABLE_UVAR_XKEY),
	undef, "Deleting a known key with conversion disabled fails");
    is (keys %$hash, keys %$placebo);

    is (XS::APItest::Hash::delete ($hash, $victim, 0),
	delete $placebo->{$victim},
	"Deleting a known key with conversion enabled works");
    is (keys %$hash, keys %$placebo);
    @keys = sort keys %$hash;
    is ("@keys", join(' ', sort($mapping->(keys %$placebo))));

    my ($k, $v) = splice @$new, 0, 2;
    $hash->{$k} = $v;
    $placebo->{$k} = $v;
    is (keys %$hash, keys %$placebo);
    @keys = sort keys %$hash;
    is ("@keys", join(' ', sort($mapping->(keys %$placebo))));

    ($k, $v) = splice @$new, 0, 2;
    is (XS::APItest::Hash::store_ent($hash, $k, $v), $v, "store_ent");
    $placebo->{$k} = $v;
    is (keys %$hash, keys %$placebo);
    @keys = sort keys %$hash;
    is ("@keys", join(' ', sort($mapping->(keys %$placebo))));

    ($k, $v) = splice @$new, 0, 2;
    is (XS::APItest::Hash::store($hash, $k, $v), $v, "store");
    $placebo->{$k} = $v;
    is (keys %$hash, keys %$placebo);
    @keys = sort keys %$hash;
    is ("@keys", join(' ', sort($mapping->(keys %$placebo))));

    @hitlist = keys %$placebo;
    $victim = shift @hitlist;
    is (XS::APItest::Hash::fetch_ent($hash, $victim), $placebo->{$victim},
	"fetch_ent");
    is (XS::APItest::Hash::fetch_ent($hash, $mapping->($victim)), undef,
	"fetch_ent (missing)");

    $victim = shift @hitlist;
    is (XS::APItest::Hash::fetch($hash, $victim), $placebo->{$victim},
	"fetch");
    is (XS::APItest::Hash::fetch($hash, $mapping->($victim)), undef,
	"fetch (missing)");

    $victim = shift @hitlist;
    ok (XS::APItest::Hash::exists_ent($hash, $victim), "exists_ent");
    ok (!XS::APItest::Hash::exists_ent($hash, $mapping->($victim)),
	"exists_ent (missing)");

    $victim = shift @hitlist;
    die "Need a victim" unless defined $victim;
    ok (XS::APItest::Hash::exists($hash, $victim), "exists");
    ok (!XS::APItest::Hash::exists($hash, $mapping->($victim)),
	"exists (missing)");

    is (XS::APItest::Hash::common({hv => $hash, keysv => $victim}),
	$placebo->{$victim}, "common (fetch)");
    is (XS::APItest::Hash::common({hv => $hash, keypv => $victim}),
	$placebo->{$victim}, "common (fetch pv)");
    is (XS::APItest::Hash::common({hv => $hash, keysv => $victim,
				   action => XS::APItest::HV_DISABLE_UVAR_XKEY}),
	undef, "common (fetch) missing");
    is (XS::APItest::Hash::common({hv => $hash, keypv => $victim,
				   action => XS::APItest::HV_DISABLE_UVAR_XKEY}),
	undef, "common (fetch pv) missing");
    is (XS::APItest::Hash::common({hv => $hash, keysv => $mapping->($victim),
				   action => XS::APItest::HV_DISABLE_UVAR_XKEY}),
	$placebo->{$victim}, "common (fetch) missing mapped");
    is (XS::APItest::Hash::common({hv => $hash, keypv => $mapping->($victim),
				   action => XS::APItest::HV_DISABLE_UVAR_XKEY}),
	$placebo->{$victim}, "common (fetch pv) missing mapped");
}

sub main_tests {
  my ($keys, $testkeys, $description) = @_;
  foreach my $key (@$testkeys) {
    my $lckey = ($key eq chr 198) ? chr 230 : lc $key;
    my $unikey = $key;
    utf8::encode $unikey;

    utf8::downgrade $key, 1;
    utf8::downgrade $lckey, 1;
    utf8::downgrade $unikey, 1;
    main_test_inner ($key, $lckey, $unikey, $keys, $description);

    utf8::upgrade $key;
    utf8::upgrade $lckey;
    utf8::upgrade $unikey;
    main_test_inner ($key, $lckey, $unikey, $keys,
		     $description . ' [key utf8 on]');
  }

  # hv_exists was buggy for tied hashes, in that the raw utf8 key was being
  # used - the utf8 flag was being lost.
  perform_test (\&test_absent, (chr 258), $keys, '');

  perform_test (\&test_fetch_absent, (chr 258), $keys, '');
  perform_test (\&test_delete_absent, (chr 258), $keys, '');
}

sub main_test_inner {
  my ($key, $lckey, $unikey, $keys, $description) = @_;
  perform_test (\&test_present, $key, $keys, $description);
  perform_test (\&test_fetch_present, $key, $keys, $description);
  perform_test (\&test_delete_present, $key, $keys, $description);

  perform_test (\&test_store, $key, $keys, $description, [a=>'cheat']);
  perform_test (\&test_store, $key, $keys, $description, []);

  perform_test (\&test_absent, $lckey, $keys, $description);
  perform_test (\&test_fetch_absent, $lckey, $keys, $description);
  perform_test (\&test_delete_absent, $lckey, $keys, $description);

  return if $unikey eq $key;

  perform_test (\&test_absent, $unikey, $keys, $description);
  perform_test (\&test_fetch_absent, $unikey, $keys, $description);
  perform_test (\&test_delete_absent, $unikey, $keys, $description);
}

sub perform_test {
  my ($test_sub, $key, $keys, $message, @other) = @_;
  my $printable = join ',', map {ord} split //, $key;

  my (%hash, %tiehash);
  tie %tiehash, 'Tie::StdHash';

  @hash{@$keys} = @$keys;
  @tiehash{@$keys} = @$keys;

  &$test_sub (\%hash, $key, $printable, $message, @other);
  &$test_sub (\%tiehash, $key, $printable, "$message tie", @other);
}

sub test_present {
  my ($hash, $key, $printable, $message) = @_;

  ok (exists $hash->{$key}, "hv_exists_ent present$message $printable");
  ok (XS::APItest::Hash::exists ($hash, $key),
      "hv_exists present$message $printable");
}

sub test_absent {
  my ($hash, $key, $printable, $message) = @_;

  ok (!exists $hash->{$key}, "hv_exists_ent absent$message $printable");
  ok (!XS::APItest::Hash::exists ($hash, $key),
      "hv_exists absent$message $printable");
}

sub test_delete_present {
  my ($hash, $key, $printable, $message) = @_;

  my $copy = {};
  my $class = tied %$hash;
  if (defined $class) {
    tie %$copy, ref $class;
  }
  $copy = {%$hash};
  ok (brute_force_exists ($copy, $key),
      "hv_delete_ent present$message $printable");
  is (delete $copy->{$key}, $key, "hv_delete_ent present$message $printable");
  ok (!brute_force_exists ($copy, $key),
      "hv_delete_ent present$message $printable");
  $copy = {%$hash};
  ok (brute_force_exists ($copy, $key),
      "hv_delete present$message $printable");
  is (XS::APItest::Hash::delete ($copy, $key), $key,
      "hv_delete present$message $printable");
  ok (!brute_force_exists ($copy, $key),
      "hv_delete present$message $printable");
}

sub test_delete_absent {
  my ($hash, $key, $printable, $message) = @_;

  my $copy = {};
  my $class = tied %$hash;
  if (defined $class) {
    tie %$copy, ref $class;
  }
  $copy = {%$hash};
  is (delete $copy->{$key}, undef, "hv_delete_ent absent$message $printable");
  $copy = {%$hash};
  is (XS::APItest::Hash::delete ($copy, $key), undef,
      "hv_delete absent$message $printable");
}

sub test_store {
  my ($hash, $key, $printable, $message, $defaults) = @_;
  my $HV_STORE_IS_CRAZY = 1;

  # We are cheating - hv_store returns NULL for a store into an empty
  # tied hash. This isn't helpful here.

  my $class = tied %$hash;

  # It's important to do this with nice new hashes created each time round
  # the loop, rather than hashes in the pad, which get recycled, and may have
  # xhv_array non-NULL
  my $h1 = {@$defaults};
  my $h2 = {@$defaults};
  if (defined $class) {
    tie %$h1, ref $class;
    tie %$h2, ref $class;
    if ($] > 5.009) {
      # bug 36327 is fixed
      $HV_STORE_IS_CRAZY = undef;
    } else {
      # HV store_ent returns 1 if there was already underlying hash storage
      $HV_STORE_IS_CRAZY = undef unless @$defaults;
    }
  }
  is (XS::APItest::Hash::store_ent($h1, $key, 1), $HV_STORE_IS_CRAZY,
      "hv_store_ent$message $printable");
  ok (brute_force_exists ($h1, $key), "hv_store_ent$message $printable");
  is (XS::APItest::Hash::store($h2, $key,  1), $HV_STORE_IS_CRAZY,
      "hv_store$message $printable");
  ok (brute_force_exists ($h2, $key), "hv_store$message $printable");
}

sub test_fetch_present {
  my ($hash, $key, $printable, $message) = @_;

  is ($hash->{$key}, $key, "hv_fetch_ent present$message $printable");
  is (XS::APItest::Hash::fetch ($hash, $key), $key,
      "hv_fetch present$message $printable");
}

sub test_fetch_absent {
  my ($hash, $key, $printable, $message) = @_;

  is ($hash->{$key}, undef, "hv_fetch_ent absent$message $printable");
  is (XS::APItest::Hash::fetch ($hash, $key), undef,
      "hv_fetch absent$message $printable");
}

sub brute_force_exists {
  my ($hash, $key) = @_;
  foreach (keys %$hash) {
    return 1 if $key eq $_;
  }
  return 0;
}

sub rot13 {
    my @results = map {my $a = $_; $a =~ tr/A-Za-z/N-ZA-Mn-za-m/; $a} @_;
    wantarray ? @results : $results[0];
}

sub bitflip {
    my @results = map {join '', map {chr(32 ^ ord $_)} split '', $_} @_;
    wantarray ? @results : $results[0];
}
