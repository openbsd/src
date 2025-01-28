use Test2::Bundle::Extended;
use Test2::Mock;

my $mock;

ok( lives {
        $mock = Test2::Mock->new(
            class => 'Fake',
            add   => [
                foo => 'string',
                bar => undef,
            ],
        );
    },
    'Did not die when adding plain value'
);

isa_ok(
    $mock,
    'Test2::Mock'
);

is( Fake::foo(),
    'string',
    'Correct value returned for add when plain string given' 
);

is( Fake::bar(),
    undef,
    'Correct value returned for add when undef given'
);

$mock->override(foo => undef, bar => 'string');

is( Fake::foo(),
    undef,
    'Correct value returned for override when undef given'
);

is( Fake::bar(),
    'string',
    'Correct value returned for override when plain string given'
);

done_testing;
