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


use Scalar::Util qw(openhandle);

print "1..4\n";

print "not " unless defined &openhandle;
print "ok 1\n";

my $fh = \*STDERR;
print "not " unless openhandle($fh) == $fh;
print "ok 2\n";

print "not " unless fileno(openhandle(*STDERR)) == fileno(STDERR);
print "ok 3\n";

print "not " if openhandle(CLOSED);
print "ok 4\n";

