use strict;
use warnings;

use Test2::Util qw/get_tid CAN_REALLY_FORK/;
use Test2::Bundle::Extended;
use Test2::Tools::AsyncSubtest;

imported_ok qw/async_subtest fork_subtest thread_subtest/;

sub DO_THREADS {
    return 0 unless $ENV{AUTHOR_TESTING} || $ENV{T2_DO_THREAD_TESTS};
    return Test2::AsyncSubtest->CAN_REALLY_THREAD;
}

my $ast = async_subtest foo => sub {
    ok(1, "Simple");
};
$ast->finish;

if (CAN_REALLY_FORK) {
    my $f_ast = fork_subtest foo => sub {
        ok(1, "forked $$");

        my $f2_ast = fork_subtest bar => sub {
            ok(1, "forked again $$");
        };

        $f2_ast->finish;
    };

    $f_ast->finish;
}

if (DO_THREADS()) {
    my $t_ast = thread_subtest foo => sub {
        ok(1, "threaded " . get_tid);

        my $t2_ast = thread_subtest bar => sub {
            ok(1, "threaded again " . get_tid);
        };

        $t2_ast->finish;
    };

    $t_ast->finish;
}

done_testing;
