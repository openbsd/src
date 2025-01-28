use Test2::Bundle::Extended -target => 'Test2::Todo';

my $todo = Test2::Todo->new(reason => 'xyz');
def isa_ok => ($todo, $CLASS);
def ok => ((grep {$_->{code} == $todo->_filter} @{Test2::API::test2_stack->top->_pre_filters}), "filter added");
def is => ($todo->reason, 'xyz', "got reason");
def ref_is => ($todo->hub, Test2::API::test2_stack->top, "used current hub");
def ok => (my $filter = $todo->_filter, "stored filter");
$todo->end;

do_def;
ok(!(grep {$_->{code} == $filter} @{Test2::API::test2_stack->top->_pre_filters}), "filter removed");

my $ok   = Test2::Event::Ok->new(pass => 0, name => 'xxx');
my $diag = Test2::Event::Diag->new(message => 'xxx');

ok(!$ok->todo, "ok is not TODO");
ok(!$ok->effective_pass, "not effectively passing");
my $filtered_ok = $filter->(Test2::API::test2_stack->top, $ok);
is($filtered_ok->todo, 'xyz', "the ok is now todo");
ok($filtered_ok->effective_pass, "now effectively passing");

isa_ok($diag, 'Test2::Event::Diag');
my $filtered_diag = $filter->(Test2::API::test2_stack->top, $diag);
isa_ok($filtered_diag, 'Test2::Event::Note');
is($filtered_diag->message, $diag->message, "new note has the same message");

my $events = intercept {
    ok(0, 'fail');

    my $todo = Test2::Todo->new(reason => 'xyz');
    ok(0, 'todo fail');
    $todo = undef;

    ok(0, 'fail');
};

like(
    $events,
    array {
        event Ok => { pass => 0, effective_pass => 0, todo => DNE };
        event Diag => {};

        event Ok => { pass => 0, effective_pass => 1, todo => 'xyz' };
        event Note => {};

        event Ok => { pass => 0, effective_pass => 0, todo => DNE };
        event Diag => {};
    },
    "Got expected events"
);

$todo = $CLASS->new(reason => 'this is a todo');
$todo->end;

is("$todo", 'this is a todo', "Stringify's to the reason");
ok($todo eq 'this is a todo', "String comparison works");

done_testing;
