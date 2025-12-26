use Test2::Bundle::Extended -target => 'Test2::Tools::Ref';

{
    package Temp;
    use Test2::Tools::Ref;

    main::imported_ok(qw/ref_ok ref_is ref_is_not/);
}

like(
    intercept {
        ref_ok({});
        ref_ok({}, 'HASH', 'pass');
        ref_ok([], 'ARRAY', 'pass');
        ref_ok({}, 'ARRAY', 'fail');
        ref_ok('xxx');
        ref_ok('xxx', 'xxx');
    },
    array {
        event Ok => { pass => 1 };
        event Ok => { pass => 1 };
        event Ok => { pass => 1 };

        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => qr/'HASH\(.*\)' is not a 'ARRAY' reference/ };

        fail_events Ok => { pass => 0 };
        event Diag => { message => qr/'xxx' is not a reference/ };

        fail_events Ok => { pass => 0 };
        event Diag => { message => qr/'xxx' is not a reference/ };

        end;
    },
    "ref_ok tests"
);

my $x = [];
my $y = [];
like(
    intercept {
        ref_is($x, $x, 'same x');
        ref_is($x, $y, 'not same');

        ref_is_not($x, $y, 'not same');
        ref_is_not($y, $y, 'same y');

        ref_is('x', $x, 'no ref');
        ref_is($x, 'x', 'no ref');

        ref_is_not('x', $x, 'no ref');
        ref_is_not($x, 'x', 'no ref');

        ref_is(undef, $x, 'undef');
        ref_is($x, undef, 'undef');

        ref_is_not(undef, $x, 'undef');
        ref_is_not($x, undef, 'undef');
    },
    array {
        event Ok => sub { call pass => 1 };
        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => "'$x' is not the same reference as '$y'" };

        event Ok => sub { call pass => 1 };
        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => "'$y' is the same reference as '$y'" };

        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => "First argument 'x' is not a reference" };
        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => "Second argument 'x' is not a reference" };

        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => "First argument 'x' is not a reference" };
        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => "Second argument 'x' is not a reference" };

        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => "First argument '<undef>' is not a reference" };
        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => "Second argument '<undef>' is not a reference" };

        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => "First argument '<undef>' is not a reference" };
        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => "Second argument '<undef>' is not a reference" };

        end;
    },
    "Ref checks"
);

done_testing;
