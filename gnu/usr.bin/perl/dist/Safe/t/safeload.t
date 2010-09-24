#!perl

BEGIN {
    require Config;
    import Config;
    if ($Config{'extensions'} !~ /\bOpcode\b/) {
	print "1..0\n";
	exit 0;
    }
    # Can we load the version module ?
    eval { require version; 1 } or do {
	print "1..0 # no version.pm\n";
	exit 0;
    };
    delete $INC{"version.pm"};
}

use strict;
use Test::More;
use Safe;
plan(tests => 1);

my $c = new Safe;
$c->permit(qw(require caller entereval unpack));
my $r = $c->reval(q{ use version; 1 });
ok( defined $r, "Can load version.pm in a Safe compartment" ) or diag $@;
