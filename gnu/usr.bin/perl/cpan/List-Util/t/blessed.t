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

use Test::More tests => 11;
use Scalar::Util qw(blessed);
use vars qw($t $x);

ok(!blessed(undef),	'undef is not blessed');
ok(!blessed(1),		'Numbers are not blessed');
ok(!blessed('A'),	'Strings are not blessed');
ok(!blessed({}),	'Unblessed HASH-ref');
ok(!blessed([]),	'Unblessed ARRAY-ref');
ok(!blessed(\$t),	'Unblessed SCALAR-ref');

$x = bless [], "ABC";
is(blessed($x), "ABC",	'blessed ARRAY-ref');

$x = bless {}, "DEF";
is(blessed($x), "DEF",	'blessed HASH-ref');

$x = bless {}, "0";
cmp_ok(blessed($x), "eq", "0",	'blessed HASH-ref');

{
  my $depth;
  {
    no warnings 'redefine';
    *UNIVERSAL::can = sub { die "Burp!" if ++$depth > 2; blessed(shift) };
  }
  $x = bless {}, "DEF";
  is(blessed($x), "DEF", 'recursion of UNIVERSAL::can');
}

{
  package Broken;
  sub isa { die };
  sub can { die };

  my $obj = bless [], __PACKAGE__;
  ::is( ::blessed($obj), __PACKAGE__, "blessed on broken isa() and can()" );
}

