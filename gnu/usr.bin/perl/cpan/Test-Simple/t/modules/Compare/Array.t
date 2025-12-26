use Test2::Bundle::Extended -target => 'Test2::Compare::Array';

use lib 't/lib';

isa_ok($CLASS, 'Test2::Compare::Base');
is($CLASS->name, '<ARRAY>', "got name");

subtest construction => sub {
    my $one = $CLASS->new();
    isa_ok($one, $CLASS);
    is($one->items, {}, "created items as a hash");
    is($one->order, [], "created order as an array");

    $one = $CLASS->new(items => { 1 => 'a', 2 => 'b' });
    is($one->items, { 1 => 'a', 2 => 'b' }, "used items as specified");
    is($one->order, [ 1, 2 ], "generated order");

    like(
        dies { $CLASS->new(items => { a => 1, b => 2 }) },
        qr/All indexes listed in the 'items' hashref must be numeric/,
        "Indexes must be numeric"
    );
    like(
        dies { $CLASS->new(items => {}, order => [ 'a' ]) },
        qr/All indexes listed in the 'order' arrayref must be numeric/,
        "Indexes must be numeric"
    );

    $one = $CLASS->new(inref => ['a', 'b']);
    is($one->items, { 0 => 'a', 1 => 'b' }, "Generated items");
    is($one->order, [ 0, 1 ], "generated order");

    like(
        dies { $CLASS->new(inref => [ 'a' ], items => { 0 => 'a' }) },
        qr/Cannot specify both 'inref' and 'items'/,
        "Cannot specify inref and items"
    );
    like(
        dies { $CLASS->new(inref => [ 'a' ], order => [ 0 ]) },
        qr/Cannot specify both 'inref' and 'order'/,
        "Cannot specify inref and order"
    );
    like(
        dies { $CLASS->new(inref => { 1 => 'a' }) },
        qr/'inref' must be an array reference, got 'HASH\(.+\)'/,
        "inref must be an array"
    );
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

subtest top_index => sub {
    my $one = $CLASS->new;
    is($one->top_index, undef, "no indexes");

    $one = $CLASS->new(inref => [ 'a', 'b', 'c' ]);
    is($one->top_index, 2, "got top index");

    $one = $CLASS->new(inref => [ 'a' ]);
    is($one->top_index, 0, "got top index");

    $one = $CLASS->new(inref => [ ]);
    is($one->top_index, undef, "no indexes");

    $one = $CLASS->new(order => [ 0, 1, 2, sub { 1 }], items => { 0 => 'a', 1 => 'b', 2 => 'c' });
    is($one->top_index, 2, "got top index, despite ref");
};

subtest add_item => sub {
    my $one = $CLASS->new();

    $one->add_item('a');
    $one->add_item(1 => 'b');
    $one->add_item(3 => 'd');

    like(
        dies { $one->add_item(2 => 'c') },
        qr/elements must be added in order!/,
        "Items must be added in order"
    );

    $one->add_item(8 => 'x');
    $one->add_item('y');

    is(
        $one->items,
        { 0 => 'a', 1 => 'b', 3 => 'd', 8 => 'x', 9 => 'y' },
        "Expected items"
    );

    is($one->order, [0, 1, 3, 8, 9], "got order");
};

subtest add_filter => sub {
    my $one = $CLASS->new;

    $one->add_item('a');
    my $f = sub { grep { m/[a-z]/ } @_ };
    $one->add_filter($f);
    $one->add_item('b');

    like(
        dies { $one->add_filter },
        qr/A single coderef is required/,
        "No filter specified"
    );
    like(
        dies { $one->add_filter(1) },
        qr/A single coderef is required/,
        "Not a valid filter"
    );
    like(
        dies { $one->add_filter(undef) },
        qr/A single coderef is required/,
        "Filter must be defined"
    );
    like(
        dies { $one->add_filter(sub { 1 }, sub { 2 }) },
        qr/A single coderef is required/,
        "Too many filters"
    );
    like(
        dies { $one->add_filter({}) },
        qr/A single coderef is required/,
        "Not a coderef"
    );

    is( $one->order, [0, $f, 1], "added filter to order array");
};

subtest deltas => sub {
    my $conv = Test2::Compare->can('strict_convert');

    my %params = (exists => 1, convert => $conv, seen => {});

    my $inref = ['a', 'b'];
    my $one = $CLASS->new(inref => $inref);

    like(
        [$one->deltas(%params, got => ['a', 'b'])],
        [],
        "No delta, no diff"
    );

    like(
        [$one->deltas(%params, got => ['a'])],
        [
            {
                dne => 'got',
                id  => [ARRAY => 1],
                got => undef,
            }
        ],
        "Got the delta for the missing value"
    );

    like(
        [$one->deltas(%params, got => ['a', 'a'])],
        [
            {
                dne => DNE,
                id  => [ARRAY => 1],
                got => 'a',
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

    $one = $CLASS->new();
    $one->add_item('a');
    $one->add_filter(
        sub {
            grep { m/[a-z]/ } @_;
        }
    );
    $one->add_item('b');

    is(
        [$one->deltas(%params, got => ['a', 1, 2, 'b'])],
        [],
        "Filter worked"
    );

    like(
        [$one->deltas(%params, got => ['a', 1, 2, 'a'])],
        [
            {
                dne => DNE,
                id  => [ARRAY => 1],
                got => 'a',
                chk => {input => 'b'},
            }
        ],
        "Filter worked, but input is still wrong"
    );
};

{
  package Foo::Array;
  use base 'MyTest::Target';

  sub new {
      my $class = shift;
      bless [ @_ ] , $class;
  }
}

subtest objects_as_arrays => sub {

    my $o1 = Foo::Array->new( 'b' ) ;
    my $o2 = Foo::Array->new( 'b' ) ;

    is ( $o1, $o2, "same" );
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
