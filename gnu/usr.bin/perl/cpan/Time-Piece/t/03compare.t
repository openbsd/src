use Test;
BEGIN { plan tests => 5 }
use Time::Piece;

my @t = ('2002-01-01 00:00',
         '2002-01-01 01:20');

@t = map Time::Piece->strptime($_, '%Y-%m-%d %H:%M'), @t;

ok($t[0] < $t[1]);

ok($t[0] != $t[1]);

ok($t[0] == $t[0]);

ok($t[0] != $t[1]);

ok($t[0] <= $t[1]);

