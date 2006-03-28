#!./perl

BEGIN {
    unless (-d 'blib') {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	keys %Config; # Silence warning
	if ($Config{extensions} !~ /\bList\/Util\b/) {
	    print "1..0 # Skip: List::Util was not built\n";
	    exit 0;
	}
    }
}

use strict;
use Test::More tests => 5;
use List::Util qw(max);

my $v;

ok(defined &max, 'defined');

$v = max(1);
is($v, 1, 'single arg');

$v = max (1,2);
is($v, 2, '2-arg ordered');

$v = max(2,1);
is($v, 2, '2-arg reverse ordered');

my @a = map { rand() } 1 .. 20;
my @b = sort { $a <=> $b } @a;
$v = max(@a);
is($v, $b[-1], '20-arg random order');
