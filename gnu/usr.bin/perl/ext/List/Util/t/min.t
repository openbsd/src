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
use List::Util qw(min);

my $v;

ok(defined &min, 'defined');

$v = min(9);
is($v, 9, 'single arg');

$v = min (1,2);
is($v, 1, '2-arg ordered');

$v = min(2,1);
is($v, 1, '2-arg reverse ordered');

my @a = map { rand() } 1 .. 20;
my @b = sort { $a <=> $b } @a;
$v = min(@a);
is($v, $b[0], '20-arg random order');
