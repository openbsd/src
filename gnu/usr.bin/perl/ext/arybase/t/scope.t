use warnings; no warnings 'deprecated';
use strict;

use Test::More tests => 14;

our @t = qw(a b c d e f);

is $t[3], "d";
$[ = 3;
is $t[3], "a";
{
	is $t[3], "a";
	$[ = -1;
	is $t[3], "e";
	$[ = +0;
	is $t[3], "d";
	$[ = +1;
	is $t[3], "c";
	$[ = 0;
	is $t[3], "d";
}
is $t[3], "a";
{
	local $[ = -1;
	is $t[3], "e";
}
is $t[3], "a";
{
	($[) = -1;
	is $t[3], "e";
}
is $t[3], "a";
use t::scope_0;
is scope0_test(), "d";


is eval(q{
	$[ = 3;
	BEGIN { my $x = "foo\x{666}"; $x =~ /foo\p{Alnum}/; }
	$t[3];
}), "a";

1;
