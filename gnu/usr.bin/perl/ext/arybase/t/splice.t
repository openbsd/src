use warnings; no warnings 'deprecated';
use strict;

use Test::More tests => 23;

our @t;
our @i5 = (3, 3, 3, 3, 3);

$[ = 3;

@t = qw(a b c d e f);
is_deeply [ scalar splice @t ], [qw(f)];
is_deeply \@t, [];

@t = qw(a b c d e f);
is_deeply [ splice @t ], [qw(a b c d e f)];
is_deeply \@t, [];

@t = qw(a b c d e f);
is_deeply [ scalar splice @t, 5 ], [qw(f)];
is_deeply \@t, [qw(a b)];

@t = qw(a b c d e f);
is_deeply [ splice @t, 5 ], [qw(c d e f)];
is_deeply \@t, [qw(a b)];

@t = qw(a b c d e f);
is_deeply [ scalar splice @t, @i5 ], [qw(f)];
is_deeply \@t, [qw(a b)];

@t = qw(a b c d e f);
is_deeply [ splice @t, @i5 ], [qw(c d e f)];
is_deeply \@t, [qw(a b)];

@t = qw(a b c d e f);
is_deeply [ scalar splice @t, 5, 2 ], [qw(d)];
is_deeply \@t, [qw(a b e f)];

@t = qw(a b c d e f);
is_deeply [ splice @t, 5, 2 ], [qw(c d)];
is_deeply \@t, [qw(a b e f)];

@t = qw(a b c d e f);
is_deeply [ scalar splice @t, 5, 2, qw(x y z) ], [qw(d)];
is_deeply \@t, [qw(a b x y z e f)];

@t = qw(a b c d e f);
is_deeply [ splice @t, 5, 2, qw(x y z) ], [qw(c d)];
is_deeply \@t, [qw(a b x y z e f)];

@t = qw(a b c d e f);
splice @t, -4, 1;
is_deeply \@t, [qw(a b d e f)];

@t = qw(a b c d e f);
splice @t, 1, 1;
is_deeply \@t, [qw(a b c d f)];

$[ = -3;

@t = qw(a b c d e f);
splice @t, -3, 1;
is_deeply \@t, [qw(b c d e f)];

1;
