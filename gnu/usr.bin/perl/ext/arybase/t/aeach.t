use warnings; no warnings 'deprecated';
use strict;

BEGIN {
	if("$]" < 5.011) {
		require Test::More;
		Test::More::plan(skip_all => "no array each on this Perl");
	}
}

use Test::More tests => 2;

our @activity;

$[ = 3;

our @t0 = qw(a b c);
@activity = ();
foreach(0..5) {
	push @activity, [ each(@t0) ];
}
is_deeply \@activity, [
	[ 3, "a" ],
	[ 4, "b" ],
	[ 5, "c" ],
	[],
	[ 3, "a" ],
	[ 4, "b" ],
];

our @t1 = qw(a b c);
@activity = ();
foreach(0..5) {
	push @activity, [ scalar each(@t1) ];
}
is_deeply \@activity, [
	[ 3 ],
	[ 4 ],
	[ 5 ],
	[ undef ],
	[ 3 ],
	[ 4 ],
];

1;
