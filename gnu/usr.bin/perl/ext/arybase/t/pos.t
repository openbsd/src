use warnings; no warnings 'deprecated';
use strict;

use Test::More tests => 12;

our $t = "abcdefghi";
scalar($t =~ /abcde/g);
our $r = \$t;

$[ = 3;

is_deeply [ scalar pos($t) ], [ 8 ];
is_deeply [ pos($t) ], [ 8 ];
is_deeply [ scalar pos($$r) ], [ 8 ];
is_deeply [ pos($$r) ], [ 8 ];

scalar($t =~ /x/g);

is_deeply [ scalar pos($t) ], [ undef ];
is_deeply [ pos($t) ], [ undef ];
is_deeply [ scalar pos($$r) ], [ undef ];
is_deeply [ pos($$r) ], [ undef ];

is pos($t), undef;
pos($t) = 5;
is 0+pos($t), 5;
is pos($t), 2;
my $posr =\ pos($t);
$$posr = 4;
{
  $[ = 0;
  is 0+$$posr, 1;
}

1;
