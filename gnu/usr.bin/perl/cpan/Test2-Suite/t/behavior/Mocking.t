use Test2::Bundle::Extended -target => 'Test2::Workflow';
use Test2::Tools::Spec;

describe mock_spec => sub {
    mock Fake1 => ( add => [ check => sub { 1 } ] );

    before_all  ba => sub { mock Fake2 => ( add => [ check => sub { 2 } ])};
    before_each be => sub { mock Fake3 => ( add => [ check => sub { 3 } ])};

    is( Fake1->check, 1, "mock applies to describe block");

    around_each ae => sub {
        my $inner = shift;
        mock Fake4 => ( add => [check => sub { 4 } ]);
        $inner->();
    };

    tests the_test => sub {
        mock Fake5 => ( add => [check => sub { 5 } ]);

        is( Fake1->check, 1, "mock 1");
        is( Fake2->check, 2, "mock 2");
        is( Fake3->check, 3, "mock 3");
        is( Fake4->check, 4, "mock 4");
        is( Fake5->check, 5, "mock 5");
    };

    describe nested => sub {
        tests inner => sub {
            is( Fake1->check, 1, "mock 1");
            is( Fake2->check, 2, "mock 2");
            is( Fake3->check, 3, "mock 3");
            is( Fake4->check, 4, "mock 4");
            ok(!Fake5->can('check'), "mock 5 did not leak");
        };
    };
};

tests post => sub {
    ok(!"Fake$_"->can('check'), "mock $_ did not leak") for 1 .. 5;
};

ok(!"Fake$_"->can('check'), "mock $_ did not leak") for 1 .. 5;

done_testing;
