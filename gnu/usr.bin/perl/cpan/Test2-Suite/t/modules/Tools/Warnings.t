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
ok(!no_warnings { warn 'a' }, "warnings");

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
        warning { warn "a\n"; warn "b\n" };
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

done_testing;
