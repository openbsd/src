use Test2::Bundle::Extended -target => 'Test2::Util::Stash';

use Test2::Util::Stash qw{
    get_stash
    get_glob
    get_symbol
    parse_symbol
    purge_symbol
    slot_to_sig sig_to_slot
};

imported_ok qw{
    get_stash
    get_glob
    get_symbol
    parse_symbol
    purge_symbol
    slot_to_sig sig_to_slot
};

is(slot_to_sig('CODE'),   '&', "Code slot sigil");
is(slot_to_sig('SCALAR'), '$', "Scalar slot sigil");
is(slot_to_sig('HASH'),   '%', "Hash slot sigil");
is(slot_to_sig('ARRAY'),  '@', "Array slot sigil");

is(sig_to_slot('&'), 'CODE',   "Code slot sigil");
is(sig_to_slot('$'), 'SCALAR', "Scalar slot sigil");
is(sig_to_slot('%'), 'HASH',   "Hash slot sigil");
is(sig_to_slot('@'), 'ARRAY',  "Array slot sigil");

is(get_stash('main'), string(\%main::), "got stash");
is(get_glob('main::ok'), \*main::ok, "got glob ref");

is(
    parse_symbol("foo"),
    {
        name    => 'foo',
        sigil   => '&',
        type    => 'CODE',
        symbol  => '&main::foo',
        package => 'main',
    },
    "Parsed simple sub"
);

is(
    parse_symbol("&foo"),
    {
        name    => 'foo',
        sigil   => '&',
        type    => 'CODE',
        symbol  => '&main::foo',
        package => 'main',
    },
    "Parsed simple sub with sigil"
);

is(
    parse_symbol("&::foo"),
    {
        name    => 'foo',
        sigil   => '&',
        type    => 'CODE',
        symbol  => '&main::foo',
        package => 'main',
    },
    "Parsed ::sub with sigil"
);

is(
    parse_symbol("&Foo::Bar::foo"),
    {
        name    => 'foo',
        sigil   => '&',
        type    => 'CODE',
        symbol  => '&Foo::Bar::foo',
        package => 'Foo::Bar',
    },
    "Parsed sub with package"
);

is(
    parse_symbol('$foo'),
    {
        name    => 'foo',
        sigil   => '$',
        type    => 'SCALAR',
        symbol  => '$main::foo',
        package => 'main',
    },
    "Parsed scalar"
);

is(
    parse_symbol('%foo'),
    {
        name    => 'foo',
        sigil   => '%',
        type    => 'HASH',
        symbol  => '%main::foo',
        package => 'main',
    },
    "Parsed hash"
);

is(
    parse_symbol('@foo'),
    {
        name    => 'foo',
        sigil   => '@',
        type    => 'ARRAY',
        symbol  => '@main::foo',
        package => 'main',
    },
    "Parsed array"
);

is(
    parse_symbol('@foo', 'XYZ::ABC'),
    {
        name    => 'foo',
        sigil   => '@',
        type    => 'ARRAY',
        symbol  => '@XYZ::ABC::foo',
        package => 'XYZ::ABC',
    },
    "Parsed with custom package"
);

like(
    dies { parse_symbol('ABC::foo', 'XYZ') },
    qr/Symbol package \(ABC\) and package argument \(XYZ\) do not match/,
    "Got exception"
);

like(
    dies { parse_symbol({package => 'ABC'}, 'XYZ') },
    qr/Symbol package \(ABC\) and package argument \(XYZ\) do not match/,
    "Got exception"
);

sub xxx { 'xxx' }
our $foo = 'xyz';
ref_is(get_symbol('xxx'),  \&xxx, "got ref for &xxx");
ref_is(get_symbol('$foo'), \$foo, 'got ref for $foo');
is(get_symbol('blah'),  undef, 'no ref for &blah');
is(get_symbol('$blah'), undef, 'no ref for $blah');

purge_symbol('xxx');
ok(!__PACKAGE__->can('xxx'), "removed &xxx symbol test 1");
is(get_symbol('xxx'), undef, "removed &xxx symbol test 2");

done_testing;
