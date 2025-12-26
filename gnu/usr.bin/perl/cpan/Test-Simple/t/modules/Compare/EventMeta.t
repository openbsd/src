use Test2::Bundle::Extended -target => 'Test2::Compare::EventMeta';

use Test2::Util qw/get_tid/;

my $one = $CLASS->new();

my $trace = Test2::Util::Trace->new(frame => ['Foo', 'foo.t', 42, 'foo']);
my $Ok = Test2::Event::Ok->new(trace => $trace, pass => 1);

is($one->get_prop_file($Ok),    'foo.t',            "file");
is($one->get_prop_line($Ok),    42,                 "line");
is($one->get_prop_package($Ok), 'Foo',              "package");
is($one->get_prop_subname($Ok), 'foo',              "subname");
is($one->get_prop_debug($Ok),   'at foo.t line 42', "trace");
is($one->get_prop_pid($Ok),     $$,                 "pid");
is($one->get_prop_tid($Ok),     get_tid,            "tid");

done_testing;
