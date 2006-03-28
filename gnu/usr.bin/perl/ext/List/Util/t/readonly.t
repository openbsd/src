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
use Test::More tests => 9;

ok( readonly(1),	'number constant');

my $var = 2;

ok( !readonly($var),	'number variable');
is( $var,	2,	'no change to number variable');

ok( readonly("fred"),	'string constant');

$var = "fred";

ok( !readonly($var),	'string variable');
is( $var,	'fred',	'no change to string variable');

$var = \2;

ok( !readonly($var),	'reference to constant');
ok( readonly($$var),	'de-reference to constant');

ok( !readonly(*STDOUT),	'glob');
