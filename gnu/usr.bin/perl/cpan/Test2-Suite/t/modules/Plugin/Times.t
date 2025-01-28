use strict;
use warnings;
use Test2::API qw/intercept context/;

use Test2::Tools::Defer qw/def do_def/;

use vars qw/@CALLBACKS/;

BEGIN {
    no warnings 'redefine';
    local *Test2::API::test2_add_callback_exit = sub { push @CALLBACKS => @_ };

    require Test2::Plugin::Times;
    def ok => (!scalar(@CALLBACKS), "requiring the module does not add a callback");

    Test2::Plugin::Times->import();

    def ok => (scalar(@CALLBACKS), "importing the module does add a callback");
}

use Test2::Tools::Basic;
use Test2::Tools::Compare qw/like/;

do_def;

my $events = intercept {
    sub {
        my $ctx = context();
        $CALLBACKS[0]->($ctx);
        $ctx->release;
    }->();
};

like(
    $events->[0]->summary,
    qr/^\S+ on wallclock \([\d\.]+ usr [\d\.]+ sys \+ [\d\.]+ cusr [\d\.]+ csys = [\d\.]+ CPU\)$/,
    "Got the time info"
);

ok($events->[0]->{times}, "Got times");
ok($events->[0]->{harness_job_fields}, "Got harness job fields");

done_testing();
