use Test2::Bundle::Extended -target => 'Test2::Tools::Warnings';

{
    package Foo;
    use Test2::Tools::Warnings qw/warns warning warnings no_warnings/;
    ::imported_ok(qw/warns warning warnings no_warnings/);
}

is(warns { 0 }, 0, "no warnings");
is(warns { warn 'a' }, 1, "1 warning");
is(warns { warn 'a' for 1 .. 4 }, 4, "4 warnings");

ok(no_warnings { 0 }, "no warnings");

ok(!no_warnings { warn 'blah 1' }, "warnings");

my $es = intercept {
    ok(!no_warnings { warn "blah 2\n" }, "warnings 1");
    ok(no_warnings { warn "blah 3\n" }, "warnings 2")
};

like(
    [grep { $_->isa('Test2::Event::Diag') } @$es],
    [
        {message => qr/Failed test 'warnings 2'/},
        {message => "blah 3\n"},
    ],
    "When the test failed we got a diag about the warning, but we got no diag when it passed"
);

is(
    warnings { 0 },
    [],
    "Empty arrayref"
);

is(
    warnings { warn "a\n" for 1 .. 4 },
    [ map "a\n", 1 .. 4 ],
    "4 warnings in arrayref"
);

is(
    warning { warn "xyz\n" },
    "xyz\n",
    "Got expected warning"
);

is(
    warning { 0 },
    undef,
    "No warning"
);

my ($events, $warn);
$events = intercept {
    $warn = warning {
        scalar warning { warn "a\n"; warn "b\n" };
    };
};

like(
    $warn,
    qr/Extra warnings in warning \{ \.\.\. \}/,
    "Got warning about extra warnings"
);

like(
    $events,
    array {
        event Note => { message => "a\n" };
        event Note => { message => "b\n" };
    },
    "Got warnings as notes."
);

like(
    warning { warns { 1 } },
    qr/Useless use of warns\(\) in void context/,
    "warns in void context"
);

like(
    warning { warning { 1 } },
    qr/Useless use of warning\(\) in void context/,
    "warns in void context"
);

like(
    warning { warnings { 1 } },
    qr/Useless use of warnings\(\) in void context/,
    "warns in void context"
);

like(
    warning { no_warnings { 1 } },
    qr/Useless use of no_warnings\(\) in void context/,
    "warns in void context"
);

done_testing;
