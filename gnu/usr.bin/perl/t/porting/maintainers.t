#!./perl -w

# Test that there are no missing Maintainers in Maintainers.PL 

BEGIN {
	# This test script uses a slightly atypical invocation of the 'standard'
	# core testing setup stanza.
	# The existing porting tools which manage the Maintainers file all
	# expect to be run from the root
	# XXX that should be fixed

    chdir '..' unless -d 't';
    @INC = qw(lib Porting);
}

use strict;
use warnings;
use Maintainers qw(show_results process_options finish_tap_output);

if ($^O eq 'VMS') {
    print "1..0 # Skip: home-grown glob doesn't handle fancy patterns\n";
    exit 0;
}

{
    local @ARGV = qw|--tap-output --checkmani|;
    show_results(process_options());
}

{
    local @ARGV = qw|--tap-output --checkmani lib/ ext/|;
    show_results(process_options());
}

finish_tap_output();

# EOF
