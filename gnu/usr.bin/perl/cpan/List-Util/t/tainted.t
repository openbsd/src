#!./perl -T

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
    elsif(!grep {/blib/} @INC) {
      unshift(@INC, qw(./inc ./blib/arch ./blib/lib));
    }
}

use Test::More tests => 5;

use Scalar::Util qw(tainted);

ok( !tainted(1), 'constant number');

my $var = 2;

ok( !tainted($var), 'known variable');

my $key = (keys %ENV)[0];

ok( tainted($ENV{$key}),	'environment variable');

$var = $ENV{$key};
ok( tainted($var),	'copy of environment variable');

{
    package Tainted;
    sub TIESCALAR { bless {} }
    sub FETCH { $^X }
}

tie my $tiedvar, 'Tainted';
ok( tainted($tiedvar), 'for magic variables');
