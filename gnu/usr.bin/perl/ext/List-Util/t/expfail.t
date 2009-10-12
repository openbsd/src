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

use Test::More tests => 3;
use strict;

$List::Util::TESTING_PERL_ONLY = $List::Util::TESTING_PERL_ONLY = 1;
require Scalar::Util;

for my $func (qw(dualvar set_prototype weaken)) {
	eval { Scalar::Util->import($func); };
	like(
	    $@,
 	    qr/$func is only available with the XS/,
 	    "no pure perl $func: error raised",
	);
}
