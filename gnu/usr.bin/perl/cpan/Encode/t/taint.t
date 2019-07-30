#!/usr/bin/perl -T
use strict;
use Encode qw(encode decode);
use Scalar::Util qw(tainted);
use Test::More;
my $taint = substr($ENV{PATH},0,0);
my $str = "dan\x{5f3e}" . $taint;                 # tainted string to encode
my $bin = encode('UTF-8', $str);                  # tainted binary to decode
my @names = Encode->encodings(':all');
plan tests => 2 * @names;
for my $name (@names) {
    my ($d, $e, $s);
    eval {
        $e = encode($name, $str);
    };
  SKIP: {
      skip $@, 1 if $@;
      ok tainted($e), "encode $name";
    }
    $bin = $e.$taint if $e;
    eval {
        $d = decode($name, $bin);
    };
  SKIP: {
      skip $@, 1 if $@;
      ok tainted($d), "decode $name";
    }
}
