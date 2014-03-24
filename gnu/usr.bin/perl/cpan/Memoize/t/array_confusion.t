#!/usr/bin/perl

use lib '..';
use Memoize 'memoize', 'unmemoize';
use Test::More;

sub reff {
  return [1,2,3];

}

sub listf {
  return (1,2,3);
}

sub f17 { return 17 }

plan tests => 7;

memoize 'reff', LIST_CACHE => 'MERGE';
memoize 'listf';

$s = reff();
@a = reff();
is(scalar(@a), 1, "reff list context");

$s = listf();
@a = listf();
is(scalar(@a), 3, "listf list context");

unmemoize 'reff';
memoize 'reff', LIST_CACHE => 'MERGE';
unmemoize 'listf';
memoize 'listf';

@a = reff();
$s = reff();
is(scalar @a, 1, "reff list context");

@a = listf();
$s = listf();
is(scalar @a, 3, "listf list context");

memoize 'f17', SCALAR_CACHE => 'MERGE';
is(f17(), 17, "f17 first call");
is(f17(), 17, "f17 second call");
is(scalar(f17()), 17, "f17 scalar context call");
