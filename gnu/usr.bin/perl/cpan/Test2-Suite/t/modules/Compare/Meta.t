use Test2::Bundle::Extended -target => 'Test2::Compare::Meta';

local *convert = Test2::Compare->can('strict_convert');

subtest simple => sub {
    my $one = $CLASS->new();
    isa_ok($one, $CLASS, 'Test2::Compare::Base');
    is($one->items, [], "generated an empty items array");
    is($one->name, '<META CHECKS>', "sane name");
    is($one->verify(exists => 0), 0, "Does not verify for non-existent values");
    is($one->verify(exists => 1), 1, "always verifies for existing values");
    ok(defined $CLASS->new(items => []), "Can provide items");
};

subtest add_prop => sub {
    my $one = $CLASS->new();

    like(
        dies { $one->add_prop(undef, convert(1)) },
        qr/prop name is required/,
        "property name is required"
    );

    like(
        dies { $one->add_prop('fake' => convert(1)) },
        qr/'fake' is not a known property/,
        "Must use valid property"
    );

    like(
        dies { $one->add_prop('blessed') },
        qr/check is required/,
        "Must use valid property"
    );

    ok($one->add_prop('blessed' => convert('xxx')), "normal");
};

{
    package FooBase;

    package Foo;
    our @ISA = 'FooBase';
}

subtest deltas => sub {
    my $one = $CLASS->new();

    my $it = bless {a => 1, b => 2, c => 3}, 'Foo';

    $one->add_prop('blessed' => 'Foo');
    $one->add_prop('reftype' => 'HASH');
    $one->add_prop('isa' => 'FooBase');
    $one->add_prop('this' => exact_ref($it));
    $one->add_prop('size' => 3);

    is(
        [$one->deltas(got => $it, convert => \&convert, seen => {})],
        [],
        "Everything matches"
    );

    my $not_it = bless ['a'], 'Bar';

    like(
        [$one->deltas(got => $not_it, convert => \&convert, seen => {})],
        [
            { verified => F(), got => 'Bar' },
            { verified => F(), got => 'ARRAY' },
            { verified => F(), got => $not_it },
            { verified => F(), got => $not_it },
            { verified => F(), got => 1 },
        ],
        "Nothing matches"
    );

    like(
        [$one->deltas(got => 'a', convert => \&convert, seen => {})],
        [
            { verified => F(), got => undef },
            { verified => F(), got => undef },
            { verified => F(), got => 'a' },
            { verified => F(), got => 'a' },
            { verified => F(), got => undef },
        ],
        "Nothing matches, wrong everything"
    );
};

done_testing;
