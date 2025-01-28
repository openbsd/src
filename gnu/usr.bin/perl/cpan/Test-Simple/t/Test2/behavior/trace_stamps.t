use strict;
use warnings;

use Test2::Tools::Tiny;

use Test2::API qw{
    intercept
    test2_enable_trace_stamps
    test2_disable_trace_stamps
    test2_trace_stamps_enabled
};

test2_enable_trace_stamps();
my $events = intercept {
    ok(1, "pass");
};
ok($events->[0]->facet_data->{trace}->{stamp}, "got stamp");

test2_disable_trace_stamps();
$events = intercept {
    ok(1, "pass");
};
ok(!exists($events->[0]->facet_data->{trace}->{stamp}), "no stamp");

done_testing;
