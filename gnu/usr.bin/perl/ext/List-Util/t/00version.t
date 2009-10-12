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

use Scalar::Util ();
use List::Util ();
use List::Util::XS ();
use Test::More tests => 2;

is( $Scalar::Util::VERSION, $List::Util::VERSION, "VERSION mismatch");
my $has_xs = eval { Scalar::Util->import('dualvar'); 1 };
my $xs_version = $has_xs ? $List::Util::VERSION : undef;
is( $List::Util::XS::VERSION, $xs_version, "XS VERSION");

