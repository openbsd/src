use Test2::Bundle::Extended;

imported_ok('gen_event');

my $e = gen_event Ok => (pass => 1, name => 'foo');
my $c = event Ok => {pass => 1, name => 'foo', trace => {frame => [__PACKAGE__, __FILE__, __LINE__ - 1]}};
like($e, $c, "Generated event");

$e = gen_event '+Test2::Event::Ok' => (pass => 1, name => 'foo');
$c = event Ok => {pass => 1, name => 'foo', trace => {frame => [__PACKAGE__, __FILE__, __LINE__ - 1]}};
like($e, $c, "Generated event long-form");

done_testing;
