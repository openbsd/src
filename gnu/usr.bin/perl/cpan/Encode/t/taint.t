#!/usr/bin/perl -T
use strict;
use Encode qw(encode decode);
use Scalar::Util qw(tainted);
use Test::More;

my $str = "abc" . substr($ENV{PATH},0,0); # tainted string
my @names = Encode->encodings(':all');
plan tests => 2 * @names;
for my $name (@names){
    my $e = encode($name, $str);
    ok tainted($e), "encode $name";
    my $d = decode($name, $e);
    ok tainted($d), "decode $name";
}
