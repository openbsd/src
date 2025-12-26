use Test2::Tools::Tiny qw/ok done_testing tests/;
use Test2::Tools::AsyncSubtest;
use Test2::API qw/intercept test2_add_uuid_via/;

our %CNT;
test2_add_uuid_via(sub {
    my $type = shift;
    $CNT{$type} ||= 1;
    $type . '-' . $CNT{$type}++;
});

my $events = intercept {
    local %CNT = ();
    tests foo => sub {
        ok(1, "pass");
    };

    local %CNT = ();
    my $ast = async_subtest foo => sub {
        ok(1, "pass");
    };
    $ast->finish;
};

tests regular => sub {
    ok($events->[0]->subtest_uuid, "subtest got a subtest uuid");

    ok($events->[0]->trace->{cid},   "subtest trace got a cid");
    ok($events->[0]->trace->{hid},   "subtest trace got a hid");
    ok($events->[0]->trace->{uuid},  "subtest trace got a uuid");
    ok($events->[0]->trace->{huuid}, "subtest trace got a huuid");

    ok($events->[0]->subevents->[-1]->trace->{cid},   "subtest plan trace got a cid");
    ok($events->[0]->subevents->[-1]->trace->{hid},   "subtest plan trace got a hid");
    ok($events->[0]->subevents->[-1]->trace->{uuid},  "subtest plan trace got a uuid");
    ok($events->[0]->subevents->[-1]->trace->{huuid}, "subtest plan trace got a huuid");
};

tests async => sub {
    ok($events->[1]->subtest_uuid, "async subtest got a subtest uuid");

    ok($events->[1]->trace->{cid},   "async subtest trace got a cid");
    ok($events->[1]->trace->{hid},   "async subtest trace got a hid");
    ok($events->[1]->trace->{uuid},  "async subtest trace got a uuid");
    ok($events->[1]->trace->{huuid}, "async subtest trace got a huuid");

    ok($events->[1]->subevents->[-1]->trace->{cid},   "async subtest plan trace got a cid");
    ok($events->[1]->subevents->[-1]->trace->{hid},   "async subtest plan trace got a hid");
    ok($events->[1]->subevents->[-1]->trace->{uuid},  "async subtest plan trace got a uuid");
    ok($events->[1]->subevents->[-1]->trace->{huuid}, "async subtest plan trace got a huuid");
};

done_testing;

__END__
