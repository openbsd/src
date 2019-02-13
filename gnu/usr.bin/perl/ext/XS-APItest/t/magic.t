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

use Scalar::Util 'weaken';
eval { sv_magic(\!0, $foo) };
is $@, "", 'PERL_MAGIC_ext is permitted on read-only things';

# assigning to an array/hash with only set magic should call that magic

{
    my (@a, %h, $i);

    sv_magic_myset(\@a, $i);
    sv_magic_myset(\%h, $i);

    $i = 0;
    @a = (1,2);
    is($i, 2, "array with set magic");

    $i = 0;
    @a = ();
    is($i, 0, "array () with set magic");

    {
        local $TODO = "HVs don't call set magic - not sure if should";

        $i = 0;
        %h = qw(a 1 b 2);
        is($i, 4, "hash with set magic");
    }

    $i = 0;
    %h = qw();
    is($i, 0, "hash () with set magic");
}

done_testing;
