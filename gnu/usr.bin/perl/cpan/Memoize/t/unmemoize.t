#!/usr/bin/perl

use lib '..';
use Memoize qw(memoize unmemoize);

print "1..5\n";

eval { unmemoize('f') };	# Should fail
print (($@ ? '' : 'not '), "ok 1\n");

{ my $I = 0;
  sub u { $I++ }
}
memoize('u');
my @ur = (&u, &u, &u);
print (("@ur" eq "0 0 0") ? "ok 2\n" : "not ok 2\n");

eval { unmemoize('u') };	# Should succeed
print ($@ ? "not ok 3\n" : "ok 3\n");

@ur = (&u, &u, &u);
print (("@ur" eq "1 2 3") ? "ok 4\n" : "not ok 4\n");

eval { unmemoize('u') };	# Should fail
print ($@ ? "ok 5\n" : "not ok 5\n");

