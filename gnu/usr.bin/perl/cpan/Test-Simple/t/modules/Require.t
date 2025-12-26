use Test2::Bundle::Extended;

use Test2::Require;
pass "Loaded Test2::Require";

like(
    dies { Test2::Require->skip() },
    qr/Class 'Test2::Require' needs to implement 'skip\(\)'/,
    "skip must be overridden"
);

my $x;

{
    package Require::Foo;
    use base 'Test2::Require';
    sub skip { $x }
}

my $events = intercept {
    $x = undef;
    Require::Foo->import();
    ok(1, 'pass');
};

like(
    $events,
    array {
        event Ok => {pass => 1, name => 'pass'};
    },
    "Did not skip all"
);

$events = intercept {
    $x = "This should skip";
    Require::Foo->import();
    die "Should not get here";
};

like(
    $events,
    array {
        event Plan => {
            max       => 0,
            directive => 'SKIP',
            reason    => 'This should skip',
        };
    },
    "Skipped all"
);

done_testing;
