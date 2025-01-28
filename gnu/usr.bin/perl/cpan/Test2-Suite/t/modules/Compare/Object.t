use Test2::Bundle::Extended -target => 'Test2::Compare::Object';

subtest simple => sub {
    my $one = $CLASS->new;
    isa_ok($one, $CLASS, 'Test2::Compare::Base');

    is($one->calls, [], "got calls arrayref for free");

    is($one->name, '<OBJECT>', "Got name");

    is($one->meta_class, 'Test2::Compare::Meta', "Correct metaclass");

    is($one->object_base, 'UNIVERSAL', "Correct object base");

    ok(defined $CLASS->new(calls => []), "Can specify a calls array")
};

subtest verify => sub {
    my $one = $CLASS->new;

    ok(!$one->verify(exists => 0), "nothing to verify");
    ok(!$one->verify(exists => 1, got => 1), "not a ref");
    ok(!$one->verify(exists => 1, got => {}), "not blessed");

    ok($one->verify(exists => 1, got => bless({}, 'Foo')), "Blessed");

    no warnings 'once';
    local *Foo::isa = sub { 0 };
    ok(!$one->verify(exists => 1, got => bless({}, 'Foo')), "not a 'UNIVERSAL' (pretend)");
};

subtest add_prop => sub {
    my $one = $CLASS->new();

    ok(!$one->meta, "no meta yet");
    $one->add_prop('blessed' => 'Foo');
    isa_ok($one->meta, 'Test2::Compare::Meta');
    is(@{$one->meta->items}, 1, "1 item");

    $one->add_prop('reftype' => 'HASH');
    is(@{$one->meta->items}, 2, "2 items");
};

subtest add_field => sub {
    my $one = $CLASS->new();

    ok(!$one->refcheck, "no refcheck yet");
    $one->add_field(foo => 1);
    isa_ok($one->refcheck, 'Test2::Compare::Hash');
    is(@{$one->refcheck->order}, 1, "1 item");

    $one->add_field(bar => 1);
    is(@{$one->refcheck->order}, 2, "2 items");

    $one = $CLASS->new();
    $one->add_item(0 => 'foo');
    like(
        dies { $one->add_field(foo => 1) },
        qr/Underlying reference does not have fields/,
        "Cannot add fields to a non-hash refcheck"
    );
};

subtest add_item => sub {
    my $one = $CLASS->new();

    ok(!$one->refcheck, "no refcheck yet");
    $one->add_item(0 => 'foo');
    isa_ok($one->refcheck, 'Test2::Compare::Array');
    is(@{$one->refcheck->order}, 1, "1 item");

    $one->add_item(1 => 'bar');
    is(@{$one->refcheck->order}, 2, "2 items");

    $one = $CLASS->new();
    $one->add_field('foo' => 1);
    like(
        dies { $one->add_item(0 => 'foo') },
        qr/Underlying reference does not have items/,
        "Cannot add items to a non-array refcheck"
    );
};

subtest add_call => sub {
    my $one = $CLASS->new;

    my $code = sub { 1 };

    $one->add_call(foo => 'FOO');
    $one->add_call($code, 1);
    $one->add_call($code, 1, 'custom');
    $one->add_call($code, 1, 'custom', 'list');

    is(
        $one->calls,
        [
            ['foo', 'FOO', 'foo',    'scalar'],
            [$code, 1,     '\&CODE', 'scalar'],
            [$code, 1,     'custom', 'scalar'],
            [$code, 1,     'custom', 'list'],
        ],
        "Added all 4 calls"
    );
};

{
    package Foo;

    package Foo::Bar;
    our @ISA = 'Foo';

    sub foo { 'foo' }
    sub baz { 'baz' }
    sub one { 1 }
    sub many { return (1,2,3,4) }
    sub args { shift; +{@_} }

    package Fake::Fake;

    sub foo { 'xxx' }
    sub one { 2 }
    sub args { shift; +[@_] }
}

subtest deltas => sub {
    my $convert = Test2::Compare->can('strict_convert');

    my $good = bless { a => 1 }, 'Foo::Bar';
    my $bad  = bless [ 'a', 1 ], 'Fake::Fake';

    my $one = $CLASS->new;
    $one->add_field(a => 1);
    $one->add_prop(blessed => 'Foo::Bar');
    $one->add_prop(isa => 'Foo');

    $one->add_call(sub {
        my $self = shift;
        die "XXX" unless $self->isa('Foo::Bar');
        'live';
    }, 'live', 'maybe_throw');

    $one->add_call('foo' => 'foo');
    $one->add_call('baz' => 'baz');
    $one->add_call('one' => 1);
    $one->add_call('many' => [1,2,3,4],undef,'list');
    $one->add_call('many' => {1=>2,3=>4},undef,'hash');
    $one->add_call([args => 1,2] => {1=>2});

    is(
        [$one->deltas(exists => 1, got => $good, convert => $convert, seen => {})],
        [],
        "Nothing failed"
    );

    like(
        [$one->deltas(got => $bad, convert => $convert, seen => {})],
        [
            {
                chk => T(),
                got => 'Fake::Fake',
                id  => ['META' => 'blessed'],
            },
            {
                chk => T(),
                got => T(),
                id  => ['META' => 'isa'],
            },
            {
                chk       => T(),
                got       => undef,
                id        => [METHOD => 'maybe_throw'],
                exception => qr/XXX/,
            },
            {
                chk => T(),
                got => 'xxx',
                id  => [METHOD => 'foo'],
            },
            {
                chk => T(),
                dne => 'got',
                got => undef,
                id  => [METHOD => 'baz'],
            },
            {
                chk => T(),
                got => 2,
                id  => [METHOD => 'one'],
            },
            {
                chk => T(),
                dne => 'got',
                got => undef,
                id  => [METHOD => 'many'],
            },
            {
                chk => T(),
                dne => 'got',
                got => undef,
                id  => [METHOD => 'many'],
            },
            {
                chk => T(),
                got => [1,2],
                id  => [METHOD => 'args'],
            },
            {
                chk => T(),
                got => [],
                id  => [META => 'Object Ref'],
            },
        ],
        "Everything failed"
    );

    # This is critical, there were a couple bugs only seen when wrapped in
    # 'run' instead of directly calling 'deltas'
    like(
        [$one->run(id => undef, got => $bad, convert => $convert, seen => {})],
        [
            {
                verified => 1,
                children => [
                    {
                        chk => T(),
                        got => 'Fake::Fake',
                        id  => ['META' => 'blessed'],
                    },
                    {
                        chk => T(),
                        got => T(),
                        id  => ['META' => 'isa'],
                    },
                    {
                        chk       => T(),
                        got       => undef,
                        id        => [METHOD => 'maybe_throw'],
                        exception => qr/XXX/,
                    },
                    {
                        chk => T(),
                        got => 'xxx',
                        id  => [METHOD => 'foo'],
                    },
                    {
                        chk => T(),
                        dne => 'got',
                        got => undef,
                        id  => [METHOD => 'baz'],
                    },
                    {
                        chk => T(),
                        got => 2,
                        id  => [METHOD => 'one'],
                    },
                    {
                        chk => T(),
                        dne => 'got',
                        got => undef,
                        id  => [METHOD => 'many'],
                    },
                    {
                        chk => T(),
                        dne => 'got',
                        got => undef,
                        id  => [METHOD => 'many'],
                    },
                    {
                        chk => T(),
                        got => [1,2],
                        id  => [METHOD => 'args'],
                    },
                    {
                        chk => T(),
                        got => [],
                        id  => [META => 'Object Ref'],
                    },
                ],
            },
        ],
        "Everything failed, check when wrapped"
    );
};

done_testing;
