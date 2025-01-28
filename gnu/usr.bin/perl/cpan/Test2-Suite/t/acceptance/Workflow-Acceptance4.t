use Test2::Bundle::Extended;
use Test2::Tools::Spec;

use Test2::API qw/intercept/;

my $unit = tests simple => sub {
    ok(1, "inside simple");
};

my $runner = Test2::Workflow::Runner->new;
$runner->push_task($unit);
$runner->run;

done_testing;
