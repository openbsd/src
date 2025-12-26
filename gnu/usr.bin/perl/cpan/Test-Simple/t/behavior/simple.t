use strict;
use warnings;

use Test2::Bundle::Extended;
use Test2::Tools::AsyncSubtest;

my $ast = async_subtest foo => sub {
    ok(1, "Simple");
};
$ast->finish;

done_testing;
