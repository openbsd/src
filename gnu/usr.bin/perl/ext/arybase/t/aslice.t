use warnings; no warnings 'deprecated', 'experimental::lexical_topic';
use strict;

use Test::More tests => 18;

our @t = qw(a b c d e f);
our $r = \@t;
our @i4 = (3, 5, 3, 5);

$[ = 3;

is_deeply [ scalar @t[3,4] ], [ qw(b) ];
is_deeply [ @t[3,4,8,9] ], [ qw(a b f), undef ];
is_deeply [ scalar @t[@i4] ], [ qw(c) ];
is_deeply [ @t[@i4] ], [ qw(a c a c) ];
is_deeply [ scalar @{$r}[3,4] ], [ qw(b) ];
is_deeply [ @{$r}[3,4,8,9] ], [ qw(a b f), undef ];
is_deeply [ scalar @{$r}[@i4] ], [ qw(c) ];
is_deeply [ @{$r}[@i4] ], [ qw(a c a c) ];

is_deeply [ @t[2,-1,1,-2] ], [ qw(f f e e) ];
{
 $[ = -3;
 is_deeply [@t[-3,()]], ['a'];
}

SKIP: {
	skip "no lexical \$_", 8 unless eval q{my $_; 1};
	eval q{
		my $_;
		is_deeply [ scalar @t[3,4] ], [ qw(b) ];
		is_deeply [ @t[3,4,8,9] ], [ qw(a b f), undef ];
		is_deeply [ scalar @t[@i4] ], [ qw(c) ];
		is_deeply [ @t[@i4] ], [ qw(a c a c) ];
		is_deeply [ scalar @{$r}[3,4] ], [ qw(b) ];
		is_deeply [ @{$r}[3,4,8,9] ], [ qw(a b f), undef ];
		is_deeply [ scalar @{$r}[@i4] ], [ qw(c) ];
		is_deeply [ @{$r}[@i4] ], [ qw(a c a c) ];
	};
}

1;
