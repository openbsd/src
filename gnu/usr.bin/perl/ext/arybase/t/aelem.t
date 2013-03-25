use warnings; no warnings 'deprecated';
use strict;

use Test::More tests => 33;

our @t = qw(a b c d e f);
our $r = \@t;
our($i3, $i4, $i8, $i9) = (3, 4, 8, 9);
our @i4 = (3, 3, 3, 3);

$[ = 3;

is $t[3], "a";
is $t[4], "b";
is $t[8], "f";
is $t[9], undef;
is_deeply [ scalar $t[4] ], [ "b" ];
is_deeply [ $t[4] ], [ "b" ];

is $t[2], 'f';
is $t[-1], 'f';
is $t[1], 'e';
is $t[-2], 'e';

{
 $[ = -3;
 is $t[-3], 'a';
}

is $r->[3], "a";
is $r->[4], "b";
is $r->[8], "f";
is $r->[9], undef;
is_deeply [ scalar $r->[4] ], [ "b" ];
is_deeply [ $r->[4] ], [ "b" ];

is $t[$i3], "a";
is $t[$i4], "b";
is $t[$i8], "f";
is $t[$i9], undef;
is_deeply [ scalar $t[$i4] ], [ "b" ];
is_deeply [ $t[$i4] ], [ "b" ];
is_deeply [ scalar $t[@i4] ], [ "b" ];
is_deeply [ $t[@i4] ], [ "b" ];

is $r->[$i3], "a";
is $r->[$i4], "b";
is $r->[$i8], "f";
is $r->[$i9], undef;
is_deeply [ scalar $r->[$i4] ], [ "b" ];
is_deeply [ $r->[$i4] ], [ "b" ];
is_deeply [ scalar $r->[@i4] ], [ "b" ];
is_deeply [ $r->[@i4] ], [ "b" ];


1;
