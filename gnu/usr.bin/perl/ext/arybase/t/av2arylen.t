use warnings; no warnings 'deprecated';
use strict;

use Test::More tests => 8;

our @t = qw(a b c d e f);
our $r = \@t;

$[ = 3;

is_deeply [ scalar $#t ], [ 8 ];
is_deeply [ $#t ], [ 8 ];
is_deeply [ scalar $#$r ], [ 8 ];
is_deeply [ $#$r ], [ 8 ];

my $arylen=\$#t;
push @t, 'g';
is 0+$$arylen, 9;
$[ = 4;
is 0+$$arylen, 10;
--$$arylen;
$[ = 3;
is 0+$$arylen, 8;
is 0+$#t, 8;

1;
