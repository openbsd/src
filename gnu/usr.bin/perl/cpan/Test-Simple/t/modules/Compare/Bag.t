use Test2::Bundle::Extended -target => 'Test2::Compare::Bag';

isa_ok($CLASS, 'Test2::Compare::Base');
is($CLASS->name, '<BAG>', "got name");

subtest construction => sub {
    my $one = $CLASS->new();
    isa_ok($one, $CLASS);
    is($one->items, [], "created items as an array");
};

subtest verify => sub {
    my $one = $CLASS->new;

    is($one->verify(exists => 0), 0, "did not get anything");
    is($one->verify(exists => 1, got => undef), 0, "undef is not an array");
    is($one->verify(exists => 1, got => 0), 0, "0 is not an array");
    is($one->verify(exists => 1, got => 1), 0, "1 is not an array");
    is($one->verify(exists => 1, got => 'string'), 0, "'string' is not an array");
    is($one->verify(exists => 1, got => {}), 0, "a hash is not an array");
    is($one->verify(exists => 1, got => []), 1, "an array is an array");
};

subtest add_item => sub {
    my $one = $CLASS->new();

    $one->add_item('a');
    $one->add_item(1 => 'b');
    $one->add_item(3 => 'd');

    ok(
        lives { $one->add_item(2 => 'c') },
        "Indexes are ignored",
    );

    $one->add_item(8 => 'x');
    $one->add_item('y');

    is(
        $one->items,
        [ 'a', 'b', 'd', 'c', 'x', 'y' ],
        "Expected items",
    );
};

subtest deltas => sub {
    my $conv = Test2::Compare->can('strict_convert');

    my %params = (exists => 1, convert => $conv, seen => {});

    my $items = ['a', 'b'];
    my $one = $CLASS->new(items => $items);

    like(
        [$one->deltas(%params, got => ['a', 'b'])],
        [],
        "No delta, no diff"
    );

    like(
        [$one->deltas(%params, got => ['b', 'a'])],
        [],
        "No delta, no diff, order is ignored"
    );

    like(
        [$one->deltas(%params, got => ['a'])],
        [
            {
                dne => 'got',
                id  => [ARRAY => '*'],
                got => undef,,
                chk => {input => 'b'},
            }
        ],
        "Got the delta for the missing value"
    );

    like(
        [$one->deltas(%params, got => ['a', 'a'])],
        [
            {
                dne => 'got',
                id  => [ARRAY => '*'],
                got => undef,
                chk => {input => 'b'},
            }
        ],
        "Got the delta for the incorrect value"
    );

    like(
        [$one->deltas(%params, got => ['a', 'b', 'a', 'a'])],
        [],
        "No delta, not checking ending"
    );

    $one->set_ending(1);
    like(
        [$one->deltas(%params, got => ['a', 'b', 'a', 'x'])],
        array {
            item 0 => {
                dne   => 'check',
                id    => [ARRAY => 2],
                got   => 'a',
                check => DNE,
            };
            item 1 => {
                dne   => 'check',
                id    => [ARRAY => 3],
                got   => 'x',
                check => DNE,
            };
            end(),
        },
        "Got 2 deltas for extra items"
    );

    subtest 'duplicate items' => sub {
        my $items = ['a', 'a'];
        my $one = $CLASS->new(items => $items);

        like(
            [$one->deltas(%params, got => ['a', 'a'])],
            [],
            "No delta, no diff"
        );

        like(
            [$one->deltas(%params, got => ['a', 'a', 'a'])],
            [],
            "No delta, not checking ending"
        );

        $one->set_ending(1);
        like(
            [$one->deltas(%params, got => ['a', 'a', 'a'])],
            array {
                item 0 => {
                    dne   => 'check',
                    id    => [ARRAY => 2],
                    got   => 'a',
                    check => DNE,
                };
                end(),
            },
            "Got the delta for extra item"
        );
    };
};

subtest add_prop => sub {
    my $one = $CLASS->new();

    ok(!$one->meta, "no meta yet");
    $one->add_prop('size' => 1);
    isa_ok($one->meta, 'Test2::Compare::Meta');
    is(@{$one->meta->items}, 1, "1 item");

    $one->add_prop('reftype' => 'ARRAY');
    is(@{$one->meta->items}, 2, "2 items");
};

done_testing;
