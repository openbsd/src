use Test2::Bundle::Extended -target => 'Test2::AsyncSubtest::Event::Attach';
use Test2::AsyncSubtest::Event::Attach;

isa_ok($CLASS, 'Test2::Event');

can_ok($CLASS, 'id');

require Test2::AsyncSubtest::Hub;
my $hub = Test2::AsyncSubtest::Hub->new();
my $events = [];
$hub->listen(sub {
    my ($h, $e) = @_;
    push @$events => $e;
});

my $one = $CLASS->new(id => 123, trace => Test2::Util::Trace->new(frame => [__PACKAGE__, __FILE__, __LINE__]));

$one->callback($hub);

like(
    pop(@$events),
    event(Exception => sub { error => qr/Invalid AsyncSubtest attach ID: 123/ }),
    "Got exception for attached id"
);

$hub->{ast_ids}->{123} = 0;
$one->callback($hub);
is($hub->ast_ids->{123}, 1, "Filled slot");
ok(!@$events, "no events added");

$one->callback($hub);
like(
    pop(@$events),
    event(Exception => sub { error => qr/AsyncSubtest ID 123 already attached/ }),
    "Got exception for invalid id"
);

done_testing;
