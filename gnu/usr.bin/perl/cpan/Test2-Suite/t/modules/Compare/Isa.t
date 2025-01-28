use Test2::Bundle::Extended -target => 'Test2::Compare::Isa';

{
    package Foo;

    package Foo::Bar;
    our @ISA = 'Foo';

    package Baz;
}

my $isa_foo         = $CLASS->new(input => 'Foo');
my $isa_foo_bar     = $CLASS->new(input => 'Foo::Bar');
my $not_isa_foo_bar = $CLASS->new(input => 'Foo::Bar', negate => 1);

isa_ok($_, $CLASS, 'Test2::Compare::Base') for $isa_foo, $isa_foo_bar, $not_isa_foo_bar;

subtest name => sub {
    is($isa_foo->name,         'Foo',      "got expected name");
    is($isa_foo_bar->name,     'Foo::Bar', "got expected name");
    is($not_isa_foo_bar->name, 'Foo::Bar', "got expected name");
};

subtest operator => sub {
    is($isa_foo->operator,         'isa',  "got expected operator");
    is($isa_foo_bar->operator,     'isa',  "got expected operator");
    is($not_isa_foo_bar->operator, '!isa', "got expected operator");
};

subtest verify => sub {
    my $foo     = bless {}, 'Foo';
    my $foo_bar = bless {}, 'Foo::Bar';
    my $baz     = bless {}, 'Baz';

    ok(!$isa_foo->verify(exists => 0, got => undef),   'does not verify against DNE');
    ok(!$isa_foo->verify(exists => 1, got => undef),   'undef is not an instance of Foo');
    ok(!$isa_foo->verify(exists => 1, got => 42),      '42 is not an instance of Foo');
    ok($isa_foo->verify(exists => 1, got => $foo),     '$foo is an instance of Foo');
    ok($isa_foo->verify(exists => 1, got => $foo_bar), '$foo_bar is an instance of Foo');
    ok(!$isa_foo->verify(exists => 1, got => $baz),    '$baz is not an instance of Foo');

    ok(!$isa_foo_bar->verify(exists => 0, got => undef),   'does not verify against DNE');
    ok(!$isa_foo_bar->verify(exists => 1, got => undef),   'undef is not an instance of Foo::Bar');
    ok(!$isa_foo_bar->verify(exists => 1, got => 42),      '42 is not an instance of Foo::Bar');
    ok(!$isa_foo_bar->verify(exists => 1, got => $foo),    '$foo is not an instance of Foo::Bar');
    ok($isa_foo_bar->verify(exists => 1, got => $foo_bar), '$foo_bar is an instance of Foo::Bar');
    ok(!$isa_foo_bar->verify(exists => 1, got => $baz),    '$baz is not an instance of Foo::Bar');

    ok(!$not_isa_foo_bar->verify(exists => 0, got => undef),    'does not verify against DNE');
    ok($not_isa_foo_bar->verify(exists => 1, got => undef),     'undef is not an instance of Foo::Bar');
    ok($not_isa_foo_bar->verify(exists => 1, got => 42),        '42 is not an instance of Foo::Bar');
    ok($not_isa_foo_bar->verify(exists => 1, got => $foo),      '$foo is not an instance of Foo::Bar');
    ok(!$not_isa_foo_bar->verify(exists => 1, got => $foo_bar), '$foo_bar is an instance of Foo::Bar');
    ok($not_isa_foo_bar->verify(exists => 1, got => $baz),      '$baz is not an instance of Foo::Bar');
};

like(
    dies { $CLASS->new() },
    qr/input must be defined for 'Isa' check/,
    "Cannot use undef as a class name"
);

done_testing;
