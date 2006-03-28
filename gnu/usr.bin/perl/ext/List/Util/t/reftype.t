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

use Test::More tests => 23;

use Scalar::Util qw(reftype);
use vars qw($t $y $x *F);
use Symbol qw(gensym);

# Ensure we do not trigger and tied methods
tie *F, 'MyTie';

@test = (
 [ undef, 1,		'number'	],
 [ undef, 'A',		'string'	],
 [ HASH   => {},	'HASH ref'	],
 [ ARRAY  => [],	'ARRAY ref'	],
 [ SCALAR => \$t,	'SCALAR ref'	],
 [ REF    => \(\$t),	'REF ref'	],
 [ GLOB   => \*F,	'tied GLOB ref'	],
 [ GLOB   => gensym,	'GLOB ref'	],
 [ CODE   => sub {},	'CODE ref'	],
# [ IO => *STDIN{IO} ] the internal sv_reftype returns UNKNOWN
);

foreach $test (@test) {
  my($type,$what, $n) = @$test;

  is( reftype($what), $type, $n);
  next unless ref($what);

  bless $what, "ABC";
  is( reftype($what), $type, $n);

  bless $what, "0";
  is( reftype($what), $type, $n);
}

package MyTie;

sub TIEHANDLE { bless {} }
sub DESTROY {}

sub AUTOLOAD {
  warn "$AUTOLOAD called";
  exit 1; # May be in an eval
}
