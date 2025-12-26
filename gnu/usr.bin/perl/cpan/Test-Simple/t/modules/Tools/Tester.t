use Test2::V0;
use Test2::Tools::Tester qw/event_groups filter_events facets/;
use Scalar::Util qw/blessed/;

my $funky = sub {
    my $ctx = context();

    $ctx->send_event(
        Generic => (
            facet_data => {
                funk1 => {details => 'funk1'},
                funk2 => [{details => 'funk2'}, {details => 'more funk2'}],
            },
        ),
    );
    $ctx->release;
};

subtest event_groups => sub {
    my $anon = sub {
        my $ctx = context();

        $ctx->pass_and_release('foo');
    };

    my $events = intercept {
        plan 11;

        pass('pass');
        ok(1, 'pass');

        is(1, 1, "pass");
        like(1, 1, "pass");

        $anon->();
        $anon->();

        $funky->();
    };

    my $groups = event_groups($events);

    is(
        $groups,
        {
            '__NA__'              => [$events->[-1]],
            'Test2::Tools::Basic' => {
                '__ALL__' => [@{$events}[0, 1, 2]],
                'plan'    => [$events->[0]],
                'pass'    => [$events->[1]],
                'ok'      => [$events->[2]],
            },
            'Test2::Tools::Compare' => {
                '__ALL__' => [@{$events}[3, 4]],
                'is'      => [$events->[3]],
                'like'    => [$events->[4]],
            },
            'main' => {
                '__ALL__'  => [@{$events}[5, 6]],
                '__ANON__' => [@{$events}[5, 6]],
            },
        },
        "Events were grouped properly"
    );
};

subtest filter_events => sub {
    my $events = intercept {
        ok(1, "pass");
        ok(0, "fail");

        is(1, 1, "pass");
        is(1, 2, "fail");
    };

    my $basic   = filter_events $events => 'Test2::Tools::Basic';
    my $compare = filter_events $events => 'Test2::Tools::Compare';

    is(@$basic, 3, "First 2 events (and a diag) are from vasic tools");
    is(@$compare, @$events - @$basic, "Other events are from compare");

    is(
        $basic,
        [@{$events}[0, 1, 2]],
        "Verify the correct events are in the basic group"
    );

    my $basic2 = filter_events $events => qr/ok$/;
    is($basic2, $basic, "Can use a regex for a filter");
};

subtest facets => sub {
    my $events = intercept {
        ok(1, "pass");
        ok(0, "fail");
        diag "xxx";
        note "yyy";

        $funky->();

        my $it = sub {
            my $ctx = context();
            $ctx->send_event(
                Generic => (
                    facet_data => {
                        errors => [
                            {fatal => 1, details => "a fatal error", tag => 'error'},
                            {fatal => 0, details => "just an error", tag => 'error'},
                        ]
                    }
                )
            );
            $ctx->release;
        };
        $it->();
    };

    my $a_facets  = facets assert => $events;
    my $i_facets  = facets info   => $events;
    my $e1_facets = facets error  => $events;
    my $e2_facets = facets errors => $events;
    my $funk1     = facets funk1  => $events;
    my $funk2     = facets funk2  => $events;

    like(
        $a_facets,
        array {
            item { details => 'pass', pass => 1 };
            item { details => 'fail', pass => 0 };
            end;
        },
        "Got both assertions"
    );

    isa_ok($a_facets->[0], ['Test2::EventFacet::Assert'], "Blessed the facet");

    like(
        $i_facets,
        array {
            item {details => qr/Failed test/, tag => 'DIAG'};
            item {details => 'xxx',     tag => 'DIAG'};
            item {details => 'yyy',     tag => 'NOTE'};
            end;
        },
        "Got the info facets"
    );

    like(
        $e1_facets,
        array {
            item {fatal => 1, details => "a fatal error", tag => 'error'};
            item {fatal => 0, details => "just an error", tag => 'error'};
            end;
        },
        "Got error facets"
    );

    is($e1_facets, $e2_facets, "Can get facet by either the name or the key");

    is($funk1, [{details => 'funk1'}], "Can use unknown facet type");
    is($funk2, [{details => 'funk2'}, {details => 'more funk2'}], "Can use unknown list facet type");
    ok(!blessed($funk1->[0]), "Did not bless the unknown type");
};

done_testing;
