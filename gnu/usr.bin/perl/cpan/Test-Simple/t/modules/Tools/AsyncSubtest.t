use Test2::Bundle::Extended -target => 'Test2::Tools::AsyncSubtest';
use Test2::Tools::AsyncSubtest;
use Test2::Util qw/get_tid CAN_REALLY_FORK/;
use Test2::API qw/intercept/;

sub DO_THREADS {
    return 0 unless $ENV{AUTHOR_TESTING} || $ENV{T2_DO_THREAD_TESTS};
    return Test2::AsyncSubtest->CAN_REALLY_THREAD;
}

ok($INC{'Test2/IPC.pm'}, "Loaded Test2::IPC");

imported_ok(qw/async_subtest fork_subtest thread_subtest/);

sub run {
    my $ast = async_subtest('foo');
    $ast->run(sub { ok(1, "inside subtest") });
    $ast->finish;

    $ast = async_subtest foo => sub { ok(1, "inside subtest") };
    $ast->finish;

    if (CAN_REALLY_FORK) {
        $ast = fork_subtest foo => sub { ok(1, "forked subtest: $$") };
        $ast->finish;
    }

    if (DO_THREADS()) {
        $ast = thread_subtest foo => sub { ok(1, "threaded subtest: " . get_tid) };
        $ast->finish;
    }
}

run();

is(
    &intercept(\&run),
    array {
        event Subtest => sub {
            call pass => T;
            call name => 'foo';
            call subevents => array {
                event Ok => { pass => 1 };
                event Plan => { max => 1 };
            };
        } for 1 .. 2;

        event Subtest => sub {
            call pass => T;
            call name => 'foo';
            call subevents => array {
                event '+Test2::AsyncSubtest::Event::Attach' => {};
                event Ok => { pass => 1 };
                event '+Test2::AsyncSubtest::Event::Detach' => {};
                event Plan => { max => 1 };
            };
        } for grep { $_ } CAN_REALLY_FORK, DO_THREADS();
    },
    "Got expected events"
);

like(
    dies { fork_subtest('foo') },
    qr/fork_subtest requires a CODE reference as the second argument/,
    "fork_subtest needs code"
);

like(
    dies { thread_subtest('foo') },
    qr/thread_subtest requires a CODE reference as the second argument/,
    "thread_subtest needs code"
);

done_testing;
