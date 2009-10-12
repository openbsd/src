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
use List::Util qw(minstr);

my $v;

ok(defined &minstr, 'defined');

$v = minstr('a');
is($v, 'a', 'single arg');

$v = minstr('a','b');
is($v, 'a', '2-arg ordered');

$v = minstr('B','A');
is($v, 'A', '2-arg reverse ordered');

my @a = map { pack("u", pack("C*",map { int(rand(256))} (0..int(rand(10) + 2)))) } 0 .. 20;
my @b = sort { $a cmp $b } @a;
$v = minstr(@a);
is($v, $b[0], 'random ordered');
