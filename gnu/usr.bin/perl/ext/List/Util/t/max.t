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


use List::Util qw(max);

print "1..5\n";

print "not " unless defined &max;
print "ok 1\n";

print "not " unless max(1) == 1;
print "ok 2\n";

print "not " unless max(1,2) == 2;
print "ok 3\n";

print "not " unless max(2,1) == 2;
print "ok 4\n";

my @a = map { rand() } 1 .. 20;
my @b = sort { $a <=> $b } @a;
print "not " unless max(@a) == $b[-1];
print "ok 5\n";
