use Test2::V0;

{
    package Foo;
    sub foo { 1 }

    package Bar;
    push @Bar::ISA => 'Foo';
    sub bar { 1 }
}

like(
    dies { my $x = mock Bar => (around => [foo => sub { }]) },
    qr/Attempt to modify a sub that does not exist 'Bar::foo' \(Mock operates on packages, not classes, are you looking for a symbol in a parent class\?\)/,
    "Cannot wrap symbol that does not exist"
);

done_testing;
