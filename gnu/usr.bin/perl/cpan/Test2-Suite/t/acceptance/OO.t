use Test2::Bundle::Extended;
use Test2::AsyncSubtest;
use Test2::Tools::Compare qw{ array event field };
use Test2::IPC;
use Test2::Util qw/CAN_REALLY_FORK CAN_THREAD get_tid/;

sub DO_THREADS {
    return 0 unless $ENV{AUTHOR_TESTING} || $ENV{T2_DO_THREAD_TESTS};
    return Test2::AsyncSubtest->CAN_REALLY_THREAD;
}

my $wrap = Test2::AsyncSubtest->new(name => 'wrap');
$wrap->start;

my $t1 = Test2::AsyncSubtest->new(name => 't1');
my $t2 = Test2::AsyncSubtest->new(name => 't2');

$wrap->stop;

$_->run(sub {
    ok(1, "not concurrent A");
}) for $t1, $t2;

ok(1, "Something else");

if (CAN_REALLY_FORK) {
    my @pids;

    $_->run(sub {
        my $id = $_->cleave;
        my $pid = fork;
        die "Failed to fork!" unless defined $pid;
        if ($pid) {
            push @pids => $pid;
            return;
        }

        my $ok = eval {
            $_->attach($id);

            ok(1, "from proc $$");

            $_->detach();

            1
        };
        exit 0 if $ok;
        warn $@;
        exit 255;
    }) for $t1, $t2;

    waitpid($_, 0) for @pids;
}

ok(1, "Something else");

if (DO_THREADS()) {
    require threads;
    my @threads;

    $_->run(sub {
        my $id = $_->cleave;
        push @threads => threads->create(sub {
            $_->attach($id);
            ok(1, "from thread " . get_tid);
            $_->detach();
        });
    }) for $t1, $t2;

    $_->join for @threads;
}

$_->run(sub {
    ok(1, "not concurrent B");
}) for $t1, $t2;

ok(1, "Something else");

ok($wrap->pending, "Pending stuff");

$_->finish for $t1, $t2;

ok(!$wrap->pending, "Ready now");
$wrap->finish;

is(
    intercept {
        my $t = Test2::AsyncSubtest->new(name => 'will die');
        $t->run(sub { die "kaboom!\n" });
        $t->finish;
    },
    array {
        event Subtest => sub {
            field name => 'will die';
            field subevents => array {
                event Exception => sub {
                    field error => "kaboom!\n";
                };
                event Plan => sub {
                    field max => 0;
                };
            };
        };
        event Diag => sub {
            field message => match qr/\QFailed test 'will die'/;
        };
        end();
    },
    'Subtest that dies not add a diagnostic about a bad plan'
);

my $sta = Test2::AsyncSubtest->new(name => 'collapse: empty');
my $stb = Test2::AsyncSubtest->new(name => 'collapse: note only');
my $stc = Test2::AsyncSubtest->new(name => 'collapse: full');

$stb->run(sub { note "inside" });
$stc->run(sub { ok(1, "test") });

$sta->finish(collapse => 1);
$stb->finish(collapse => 1);
$stc->finish(collapse => 1);


done_testing;
