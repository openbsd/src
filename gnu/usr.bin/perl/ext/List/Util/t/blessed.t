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

use Scalar::Util qw(blessed);
use vars qw($t $y $x);

print "1..7\n";

print "not " if blessed(1);
print "ok 1\n";

print "not " if blessed('A');
print "ok 2\n";

print "not " if blessed({});
print "ok 3\n";

print "not " if blessed([]);
print "ok 4\n";

$y = \$t;

print "not " if blessed($y);
print "ok 5\n";

$x = bless [], "ABC";

print "not " unless blessed($x);
print "ok 6\n";

print "not " unless blessed($x) eq 'ABC';
print "ok 7\n";
