use Test::More tests => 7;

BEGIN { use_ok('Time::Piece'); }

my $t = gmtime(315532800); # 00:00:00 1/1/1980

isa_ok($t, 'Time::Piece', 'specific gmtime');

cmp_ok($t->year, '==', 1980, 'correct year');

cmp_ok($t->hour, '==',    0, 'correct hour');

cmp_ok($t->mon,  '==',    1, 'correct mon');

my $g = gmtime;
isa_ok($g, 'Time::Piece', 'current gmtime');

my $l = localtime;
isa_ok($l, 'Time::Piece', 'current localtime');
