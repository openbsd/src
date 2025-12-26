use Test2::Bundle::Extended -target => 'Test2::Compare::Scalar';

my $one = $CLASS->new(item => 'foo');
is($one->name, '<SCALAR>', "got name");
is($one->operator, '${...}', "Got operator");

ok(!$one->verify(exists => 0), "nothing to verify");
ok(!$one->verify(exists => 1, got => undef), "undef");
ok(!$one->verify(exists => 1, got => 'a'), "not a ref");
ok(!$one->verify(exists => 1, got => {}), "not a scalar ref");

ok($one->verify(exists => 1, got => \'anything'), "Scalar ref");

my $convert = Test2::Compare->can('strict_convert');

is(
    [$one->deltas(got => \'foo', convert => $convert, seen => {})],
    [],
    "Exact match, no delta"
);

like(
    [$one->deltas(got => \'bar', convert => $convert, seen => {})],
    [
        {
            got => 'bar',
            id  => [SCALAR => '$*'],
            chk => {'input' => 'foo'},
        }
    ],
    "Value pointed to is different"
);

like(
    dies { $CLASS->new() },
    qr/'item' is a required attribute/,
    "item is required"
);

done_testing;
