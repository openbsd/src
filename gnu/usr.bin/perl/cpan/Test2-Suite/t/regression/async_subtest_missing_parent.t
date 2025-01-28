use Test2::V0;
use Test2::Tools::AsyncSubtest;

my $err;
my $events = intercept {
    my $ast;

    subtest outer => sub {
        plan 2;
        ok(1);
        $ast = async_subtest 'foo';
        $ast->run(sub { ok(1, 'pass') });
    };

    $err = dies { $ast->finish };
};

like(
    $err,
    qr/Attempt to close AsyncSubtest when original parent hub \(a non async-subtest\?\) has ended/,
    "Throw an error when a subtest finishes without a parent"
);

done_testing;
