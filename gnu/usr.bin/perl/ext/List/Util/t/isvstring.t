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

$|=1;
require Scalar::Util;
if (grep { /isvstring/ } @Scalar::Util::EXPORT_FAIL) {
    print("1..0\n");
    exit 0;
}

Scalar::Util->import(qw[isvstring]);

print "1..4\n";

print "ok 1\n";

$vs = ord("A") == 193 ? 241.75.240 : 49.46.48;

print "not " unless $vs == "1.0";
print "ok 2\n";

print "not " unless isvstring($vs);
print "ok 3\n";

$sv = "1.0";
print "not " if isvstring($sv);
print "ok 4\n";



