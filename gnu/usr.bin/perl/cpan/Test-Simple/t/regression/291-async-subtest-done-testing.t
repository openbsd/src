use Test2::V0;

use Test2::Require::RealFork;

use Test2::Tools::AsyncSubtest qw/fork_subtest/;

my $st = fork_subtest foo => sub {
    ok(1, "Just a pass");

    like(
        warning { done_testing },
        qr/A plan should not be set inside an async-subtest \(did you call done_testing\(\)\?\)/,
        "We get a warning if we call done_testing inside an asyncsubtest"
    );
};

$st->finish;

done_testing;
