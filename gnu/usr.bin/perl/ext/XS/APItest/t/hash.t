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

use_ok('XS::APItest');

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
  is (XS::APItest::Hash::store(\%h, chr 258,  1), 1);
    
  ok (!exists $h{$utf8_for_258},
      "hv_store doesn't insert a key with the raw utf8 on a tied hash");
}

exit;

################################   The End   ################################

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
  is (delete $copy->{$key}, $key, "hv_delete_ent present$message $printable");
  $copy = {%$hash};
  is (XS::APItest::Hash::delete ($copy, $key), $key,
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

  my %h1 = @$defaults;
  my %h2 = @$defaults;
  if (defined $class) {
    tie %h1, ref $class;
    tie %h2, ref $class;
    $HV_STORE_IS_CRAZY = undef unless @$defaults;
  }
  is (XS::APItest::Hash::store_ent(\%h1, $key, 1), 1,
      "hv_store_ent$message $printable"); 
  ok (brute_force_exists (\%h1, $key), "hv_store_ent$message $printable");
  is (XS::APItest::Hash::store(\%h2, $key,  1), $HV_STORE_IS_CRAZY,
      "hv_store$message $printable");
  ok (brute_force_exists (\%h2, $key), "hv_store$message $printable");
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
