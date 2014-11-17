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

use List::Util qw(any all notall none);
use Test::More tests => 12;

ok(  (any { $_ == 1 } 1, 2, 3), 'any true' );
ok( !(any { $_ == 1 } 2, 3, 4), 'any false' );
ok( !(any { 1 }), 'any empty list' );

ok(  (all { $_ == 1 } 1, 1, 1), 'all true' );
ok( !(all { $_ == 1 } 1, 2, 3), 'all false' );
ok(  (all { 1 }), 'all empty list' );

ok(  (notall { $_ == 1 } 1, 2, 3), 'notall true' );
ok( !(notall { $_ == 1 } 1, 1, 1), 'notall false' );
ok( !(notall { 1 }), 'notall empty list' );

ok(  (none { $_ == 1 } 2, 3, 4), 'none true' );
ok( !(none { $_ == 1 } 1, 2, 3), 'none false' );
ok(  (none { 1 }), 'none empty list' );
