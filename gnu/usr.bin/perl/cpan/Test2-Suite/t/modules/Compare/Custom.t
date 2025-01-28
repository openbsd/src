use Test2::Bundle::Extended -target => 'Test2::Compare::Custom';
use Test2::API qw(intercept);

my $pass = $CLASS->new(code => sub { 1 });
my $fail = $CLASS->new(code => sub { 0 });

isa_ok($pass, $CLASS, 'Test2::Compare::Base');
isa_ok($fail, $CLASS, 'Test2::Compare::Base');

ok($pass->verify(got => "anything"), "always passes");
ok(!$fail->verify(got => "anything"), "always fails");

is($pass->operator, 'CODE(...)', "default operator");
is($pass->name, '<Custom Code>', "default name");
ok(!$pass->stringify_got, "default stringify_got");

{
    package My::String;
    use overload '""' => sub { "xxx" };
}

my $stringify = $CLASS->new(code => sub { 0 }, stringify_got => 1);
ok($stringify->stringify_got, "custom stringify_got()");
like(
    intercept {
        my $object = bless {}, 'My::String';
        is($object => $stringify);
    },
    array {
        event Fail => sub {
            call info => array {
                item hash {
                    field table => hash {
                        field rows => [['', '', 'xxx', 'CODE(...)', '<Custom Code>']];
                    };
                };
            };
        };
    },
    "stringified object in test output"
);

my $args;
my $under;
my $one = $CLASS->new(code => sub { $args = {@_}; $under = $_ }, name => 'the name', operator => 'the op');
$_ = undef;
$one->verify(got => 'foo', exists => 'x');
is($_, undef, '$_ restored');

is($args, {got => 'foo', exists => 'x', operator => 'the op', name => 'the name'}, "Got the expected args");
is($under, 'foo', '$_ was set');

like(
    dies { $CLASS->new() },
    qr/'code' is required/,
    "Need to provide code"
);

done_testing;
