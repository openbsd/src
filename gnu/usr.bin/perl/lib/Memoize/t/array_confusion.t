#!/usr/bin/perl

use lib '..';
use Memoize 'memoize', 'unmemoize';

sub reff {
  return [1,2,3];

}

sub listf {
  return (1,2,3);
}

print "1..6\n";

memoize 'reff', LIST_CACHE => 'MERGE';
print "ok 1\n";
memoize 'listf';
print "ok 2\n";

$s = reff();
@a = reff();
print @a == 1 ? "ok 3\n" : "not ok 3\n";

$s = listf();
@a = listf();
print @a == 3 ? "ok 4\n" : "not ok 4\n";

unmemoize 'reff';
memoize 'reff', LIST_CACHE => 'MERGE';
unmemoize 'listf';
memoize 'listf';

@a = reff();
$s = reff();
print @a == 1 ? "ok 5\n" : "not ok 5\n";

@a = listf();
$s = listf();
print @a == 3 ? "ok 6\n" : "not ok 6\n";


