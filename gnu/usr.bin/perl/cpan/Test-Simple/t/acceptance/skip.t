use Test2::Bundle::Extended;
use Test2::AsyncSubtest;
use Test2::Tools::AsyncSubtest;
use Test2::Tools::Compare qw{ array event call T };
use Test2::IPC;
use Test2::Util qw/CAN_REALLY_FORK/;
use Test2::API qw/context context_do intercept/;

sub DO_THREADS {
    return 0 unless $ENV{AUTHOR_TESTING} || $ENV{T2_DO_THREAD_TESTS};
    return Test2::AsyncSubtest->CAN_REALLY_THREAD;
}

skip_all 'These tests require forking or threading'
    unless CAN_REALLY_FORK || DO_THREADS();

subtest(
    'fork tests',
    sub {
        run_tests('fork');
        stress_tests('fork');
    }
) if CAN_REALLY_FORK;

subtest(
    'thread tests',
    sub {
        run_tests('thread');
        stress_tests('thread');
    }
) if DO_THREADS();

done_testing;

sub run_tests {
    my $type = shift;

    my $st_sub = $type eq 'fork' ? \&fork_subtest : \&thread_subtest;

    is(
        intercept {
            $st_sub->(
                '$ctx->plan(0, SKIP)',
                sub {
                    skip_all 'because';
                    ok(0, "Should not see");
                }
            )->finish;
        },
        array {
            event Subtest => sub {
                call name      => '$ctx->plan(0, SKIP)';
                call pass      => T();
                call subevents => array {
                    event '+Test2::AsyncSubtest::Event::Attach';
                    event Plan => sub {
                        call directive => 'SKIP';
                        call reason    => 'because';
                    };
                    event '+Test2::AsyncSubtest::Event::Detach';
                    end();
                };
            };
            end();
        },
        qq[${type}_subtest with skip_all]
    );

    is(
        intercept {
            $st_sub->(
                'skip_all',
                { manual_skip_all => 1 },
                sub {
                    skip_all 'because';
                    note "Post skip";
                    return;
                }
            )->finish;
        },
        array {
            event Subtest => sub {
                call name      => 'skip_all';
                call pass      => T();
                call subevents => array {
                    event '+Test2::AsyncSubtest::Event::Attach';
                    event Plan => sub {
                        call directive => 'SKIP';
                        call reason    => 'because';
                    };
                    event Note => { message => 'Post skip' };
                    event '+Test2::AsyncSubtest::Event::Detach';
                    end();
                };
            };
            end();
        },
        qq[${type}_subtest with skip_all and manual skip return}]
    );

    my $method = 'run_' . $type;
    is(
        intercept {
            my $at = Test2::AsyncSubtest->new(name => '$ctx->plan(0, SKIP)');
            $at->$method(
                sub {
                    skip_all 'because';
                    ok(0, "should not see");
                }
            );
            $at->finish;
        },
        array {
            event Subtest => sub {
                call name      => '$ctx->plan(0, SKIP)';
                call pass      => T();
                call subevents => array {
                    event '+Test2::AsyncSubtest::Event::Attach';
                    event Plan => sub {
                        call directive => 'SKIP';
                        call reason    => 'because';
                    };
                    event '+Test2::AsyncSubtest::Event::Detach';
                    end();
                };
            };
            end();
        },
        qq[\$subtest->$method with skip_all]
    );
}

sub stress_tests {
    my $type = shift;

    my $st_sub = $type eq 'fork' ? \&fork_subtest : \&thread_subtest;

    for my $i (2 .. 5) {
        my @st;
        for my $j (1 .. $i) {
            push @st, $st_sub->(
                "skip all $i - $j",
                sub {
                    skip_all 'because';
                    ok(0, "should not see");
                }
            );
        }
        $_->finish for @st;
    }
}
