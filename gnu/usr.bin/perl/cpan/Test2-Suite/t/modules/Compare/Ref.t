use Test2::Bundle::Extended -target => 'Test2::Compare::Ref';

my $ref = sub { 1 };
my $one = $CLASS->new(input => $ref);
isa_ok($one, $CLASS, 'Test2::Compare::Base');

like($one->name, qr/CODE\(.*\)/, "Got Name");
is($one->operator, '==', "got operator");

ok($one->verify(exists => 1, got => $ref), "verified ref");
ok(!$one->verify(exists => 1, got => sub { 1 }), "different ref");
ok(!$one->verify(exists => 0, got => $ref), "value must exist");

is(
    [ 'a', $ref ],
    [ 'a', $one ],
    "Did a ref check"
);

ok(!$one->verify(exists => 1, got => 'a'), "not a ref");

$one->set_input('a');
ok(!$one->verify(exists => 1, got => $ref), "input not a ref");

like(
    dies { $CLASS->new() },
    qr/'input' is a required attribute/,
    "Need input"
);

like(
    dies { $CLASS->new(input => 'a') },
    qr/'input' must be a reference, got 'a'/,
    "Input must be a ref"
);

done_testing;
