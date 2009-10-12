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
    if ($] eq "5.008009" or $] eq "5.010000" or $] le "5.006002") {
        print "1..0 # Skip: known to fail on $]\n";
        exit 0;
    }
}

use List::Util qw(reduce);
use Test::More tests => 1;

my $ret = "original";
$ret = $ret . broken();
is($ret, "originalreturn");

sub broken {
    reduce { return "bogus"; } qw/some thing/;
    return "return";
}
