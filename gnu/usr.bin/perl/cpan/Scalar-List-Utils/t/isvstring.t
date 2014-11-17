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
use Scalar::Util ();
use Test::More  (grep { /isvstring/ } @Scalar::Util::EXPORT_FAIL)
			? (skip_all => 'isvstring requires XS version')
			: (tests => 3);

Scalar::Util->import(qw[isvstring]);

$vs = ord("A") == 193 ? 241.75.240 : 49.46.48;

ok( $vs == "1.0",	'dotted num');
ok( isvstring($vs),	'isvstring');

$sv = "1.0";
ok( !isvstring($sv),	'not isvstring');



