#!/usr/bin/perl

use lib '..';
use Memoize 'flush_cache', 'memoize';
print "1..8\n";
print "ok 1\n";



my $V = 100;
sub VAL { $V }

memoize 'VAL';
print "ok 2\n";

my $c1 = VAL();
print (($c1 == 100) ? "ok 3\n" : "not ok 3\n");

$V = 200;
$c1 = VAL();
print (($c1 == 100) ? "ok 4\n" : "not ok 4\n");

flush_cache('VAL');
$c1 = VAL();
print (($c1 == 200) ? "ok 5\n" : "not ok 5\n");

$V = 300;
$c1 = VAL();
print (($c1 == 200) ? "ok 6\n" : "not ok 6\n");

flush_cache(\&VAL);
$c1 = VAL();
print (($c1 == 300) ? "ok 7\n" : "not ok 7\n");

$V = 400;
$c1 = VAL();
print (($c1 == 300) ? "ok 8\n" : "not ok 8\n");





