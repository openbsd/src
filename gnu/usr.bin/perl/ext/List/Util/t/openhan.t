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

use strict;
use vars qw(*CLOSED);
use Test::More tests => 4;
use Scalar::Util qw(openhandle);

ok(defined &openhandle, 'defined');

my $fh = \*STDERR;
is(openhandle($fh), $fh, 'STDERR');

is(fileno(openhandle(*STDERR)), fileno(STDERR), 'fileno(STDERR)');

is(openhandle(*CLOSED), undef, 'closed');

