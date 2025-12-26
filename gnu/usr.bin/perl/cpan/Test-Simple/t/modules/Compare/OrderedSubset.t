use Test2::Bundle::Extended -target => 'Test2::Compare::OrderedSubset';

use lib 't/lib';

isa_ok($CLASS, 'Test2::Compare::Base');
is($CLASS->name, '<ORDERED SUBSET>', "got name");

subtest construction => sub {
    my $one = $CLASS->new();
    isa_ok($one, $CLASS);
    is($one->items, [], "created items as an array");

    $one = $CLASS->new(items => [qw/a b/]);
    is($one->items, [qw/a b/], "used items as specified");

    $one = $CLASS->new(inref => ['a', 'b']);
    is($one->items, [qw/a b/], "Generated items");

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

subtest add_item => sub {
    my $one = $CLASS->new();

    $one->add_item('a');
    $one->add_item(1 => 'b');
    $one->add_item(3 => 'd');

    $one->add_item(8 => 'x');
    $one->add_item('y');

    is(
        $one->items,
        [ 'a', 'b', 'd', 'x', 'y' ],
        "Expected items"
    );
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
                id  => [ARRAY => '?'],
            }
        ],
        "Got the delta for the missing value"
    );

    like(
        [$one->deltas(%params, got => ['a', 'a'])],
        [
            {
                dne => 'got',
                id  => [ARRAY => '?'],
            }
        ],
        "Got the delta for the incorrect value"
    );

    like(
        [$one->deltas(%params, got => ['a', 'b', 'a', 'a'])],
        [],
        "No delta, not checking ending"
    );
};

{
  package Foo::OO;

  use base 'MyTest::Target';

  sub new {
      my $class = shift;
      bless [ @_ ] , $class;
  }
}

subtest object_as_arrays => sub {
    my $o1 = Foo::OO->new( 'b') ;

    is ( $o1 , subset{  item 'b' }, "same" );
};

done_testing;
