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

use Test::More tests => 2;

# force perl-only version to be tested
$List::Util::TESTING_PERL_ONLY = $List::Util::TESTING_PERL_ONLY = 1;

require Scalar::Util;
require List::Util;

is( $Scalar::Util::PP::VERSION, $List::Util::VERSION, "VERSION mismatch");
is( $List::Util::PP::VERSION, $List::Util::VERSION, "VERSION mismatch");

