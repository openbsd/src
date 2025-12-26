use Test2::Bundle::Extended -target => 'Test2::AsyncSubtest::Event::Detach';
use Test2::AsyncSubtest::Event::Detach;

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
    "Got exception for invalid id"
);

$hub->{ast_ids}->{123} = 0;
$one->callback($hub);
like(
    pop(@$events),
    event(Exception => sub { error => qr/AsyncSubtest ID 123 is not attached/ }),
    "Got exception for unattached id"
);

$hub->{ast_ids}->{123} = 1;
$one->callback($hub);
ok(!exists($hub->ast_ids->{123}), "deleted slot");
ok(!@$events, "no events added");

done_testing;
