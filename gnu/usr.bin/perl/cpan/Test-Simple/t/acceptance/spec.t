use Test2::V0 -target => 'Test2::Tools::Spec';
use Test2::Tools::Spec;

tests foo => sub {
    ok(1, "pass");
};

describe nested => sub {
    my $x = 0;

    before_all set_x => sub { $x = 100 };

    tests a => sub {
        is($x, 100, "x was set (A)");
    };

    tests b => sub {
        is($x, 100, "x was set (B)");
    };
};

done_testing;
