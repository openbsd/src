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

use Scalar::Util qw(readonly);


print "1..9\n";

print "not " unless readonly(1);
print "ok 1\n";

my $var = 2;

print "not " if readonly($var);
print "ok 2\n";

print "not " unless $var == 2;
print "ok 3\n";

print "not " unless readonly("fred");
print "ok 4\n";

$var = "fred";

print "not " if readonly($var);
print "ok 5\n";

print "not " unless $var eq "fred";
print "ok 6\n";

$var = \2;

print "not " if readonly($var);
print "ok 7\n";

print "not " unless readonly($$var);
print "ok 8\n";

print "not " if readonly(*STDOUT);
print "ok 9\n";
