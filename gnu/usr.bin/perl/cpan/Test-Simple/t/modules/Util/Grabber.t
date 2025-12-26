use Test2::Bundle::Extended -target => 'Test2::Util::Grabber';

ok(1, "initializing");

my $grab = $CLASS->new;
ok(1, "pass");
my $one = $grab->events;
ok(0, "fail");
my $events = $grab->finish;

is(@$one, 1, "Captured 1 event");
is(@$events, 3, "Captured 3 events");

like(
    $one,
    array {
        event Ok => { pass => 1 };
    },
    "Got expected event"
);

like(
    $events,
    array {
        event Ok => { pass => 1 };
        event Ok => { pass => 0 };
        event Diag => { };
        end;
    },
    "Got expected events"
);

done_testing;
