use strict;
use warnings;
use Test::More;

use XS::APItest;

my $sv = bless {}, 'Moo';
my $foo = 'affe';
my $bar = 'tiger';

ok !mg_find_foo($sv), 'no foo magic yet';
ok !mg_find_bar($sv), 'no bar magic yet';

sv_magic_foo($sv, $foo);
is mg_find_foo($sv), $foo, 'foo magic attached';
ok !mg_find_bar($sv), '... but still no bar magic';

sv_magic_bar($sv, $bar);
is mg_find_foo($sv), $foo, 'foo magic still attached';
is mg_find_bar($sv), $bar, '... and bar magic is there too';

sv_unmagic_foo($sv);
ok !mg_find_foo($sv), 'foo magic removed';
is mg_find_bar($sv), $bar, '... but bar magic is still there';

sv_unmagic_bar($sv);
ok !mg_find_foo($sv), 'foo magic still removed';
ok !mg_find_bar($sv), '... and bar magic is removed too';

is(test_get_vtbl(), 0, 'get_vtbl(-1) returns NULL');

done_testing;
