#!/usr/bin/perl

use lib 'blib/lib';
use Memoize 0.45 qw(memoize unmemoize);
use Fcntl;

# print STDERR $INC{'Memoize.pm'}, "\n";

print "1..10\n";

# Test MERGE
sub xx {
  wantarray();
}

my $s = xx();
print ((!$s) ? "ok 1\n" : "not ok 1\n");
my ($a) = xx();
print (($a) ? "ok 2\n" : "not ok 2\n");
memoize 'xx', LIST_CACHE => MERGE;
$s = xx();
print ((!$s) ? "ok 3\n" : "not ok 3\n");
($a) = xx();  # Should return cached false value from previous invocation
print ((!$a) ? "ok 4\n" : "not ok 4\n");


# Test FAULT
sub ns {}
sub na {}
memoize 'ns', SCALAR_CACHE => FAULT;
memoize 'na', LIST_CACHE => FAULT;
eval { my $s = ns() };  # Should fault
print (($@) ?  "ok 5\n" : "not ok 5\n");
eval { my ($a) = na() };  # Should fault
print (($@) ?  "ok 6\n" : "not ok 6\n");


# Test HASH
my (%s, %l);
sub nul {}
memoize 'nul', SCALAR_CACHE => [HASH => \%s], LIST_CACHE => [HASH => \%l];
nul('x');
nul('y');
print ((join '', sort keys %s) eq 'xy' ? "ok 7\n" : "not ok 7\n");
print ((join '', sort keys %l) eq ''   ? "ok 8\n" : "not ok 8\n");
() = nul('p');
() = nul('q');
print ((join '', sort keys %s) eq 'xy' ? "ok 9\n" : "not ok 9\n");
print ((join '', sort keys %l) eq 'pq' ? "ok 10\n" : "not ok 10\n");

