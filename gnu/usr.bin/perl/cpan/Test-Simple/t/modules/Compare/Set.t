use Test2::Bundle::Extended -target => 'Test2::Compare::Set';

subtest construction => sub {
    my $one = $CLASS->new();
    isa_ok($one, 'Test2::Compare::Base', $CLASS);
    is($one->reduction, 'any', "default to 'any'");
    is($one->checks, [], "default to empty list of checks");
    is($one->name, '<CHECK-SET>', "got name");
    is($one->operator, 'any', "got op");

    $one = $CLASS->new(checks => [ 'a', 'b' ], reduction => 'all');
    isa_ok($one, 'Test2::Compare::Base', $CLASS);
    is($one->reduction, 'all', "specified reduction");
    is($one->checks, ['a', 'b'], "specified checks");
    is($one->name, '<CHECK-SET>', "got name");
    is($one->operator, 'all', "got op");

    like(
        dies { $CLASS->new(reduction => 'fake') },
        qr/'fake' is not a valid set reduction/,
        "Need a valid reduction",
    );
};

subtest set_reduction => sub {
    my $one = $CLASS->new();
    is($one->reduction, 'any', "default");
    $one->set_reduction('all');
    is($one->reduction, 'all', "changed");

    like(
        dies { $one->set_reduction('fake') },
        qr/'fake' is not a valid set reduction/,
        "Need a valid reduction",
    );
};

subtest verify => sub {
    my $one = $CLASS->new();

    is($one->verify(exists => 1), 1, "valid");

    # in_set(DNE) is a valid construct, so we cannot reject non-existing values.
    is($one->verify(exists => 0), 1, "valid");
};

subtest add_check => sub {
    my $one = $CLASS->new(checks => ['a']);
    $one->add_check('b');
    $one->add_check(match qr/xxx/);

    is(
        $one->checks,
        [ 'a', 'b', meta { prop blessed => 'Test2::Compare::Pattern' } ],
        "Added the checks"
    );
};

subtest deltas => sub {
    my $one;

    my $after_each = sub {
        $one->set_checks(undef);
        is(
            dies { $one->deltas() },
            "No checks defined for set\n",
            "Need checks list"
        );

        $one->set_checks([]);
        $one->set_file(__FILE__); my $file = __FILE__;
        is(
            dies { $one->deltas() },
            "No checks defined for set\n",
            "Need checks in list"
        );

        $one->set_checks(undef);
        $one->set_lines([__LINE__]); my $line1 = __LINE__;
        is(
            dies { $one->deltas() },
            "No checks defined for set (Set defined in $file line $line1)\n",
            "Need checks list, have file+line"
        );

        $one->set_checks([]);
        push @{$one->lines} => __LINE__; my $line2 = __LINE__;
        is(
            dies { $one->deltas() },
            "No checks defined for set (Set defined in $file lines $line1, $line2)\n",
            "Need checks in list, have file + 2 lines"
        );
    };

    subtest any => sub {
        $one = $CLASS->new(reduction => 'any');
        $one->add_check(match qr/a/);
        $one->add_check(match qr/b/);
        $one->add_check(match qr/c/);

        is('xax', $one, "matches 'a'");
        is('xbx', $one, "matches 'b'");
        is('xcx', $one, "matches 'c'");

        is([$one->deltas(got => 'a', exists => 1, seen => {}, convert => sub { $_[0] })], [], "no deltas with 'a'");
        is([$one->deltas(got => 'b', exists => 1, seen => {}, convert => sub { $_[0] })], [], "no deltas with 'b'");
        is([$one->deltas(got => 'c', exists => 1, seen => {}, convert => sub { $_[0] })], [], "no deltas with 'c'");

        like(
            [$one->deltas(got => 'x', exists => 1, seen => {}, convert => sub { $_[0] })],
            [{ got => 'x' }, { got => 'x' }, { got => 'x' }, DNE],
            "no matches, 3 deltas, one per check"
        );

        $after_each->();
    };

    subtest all => sub {
        $one = $CLASS->new(reduction => 'all');
        $one->add_check(mismatch qr/x/);
        $one->add_check(match qr/fo/);
        $one->add_check(match qr/oo/);

        is('foo', $one, "matches all 3");

        is([$one->deltas(got => 'foo', exists => 1, seen => {}, convert => sub { $_[0] })], [], "no deltas with 'foo'");

        like(
            [$one->deltas(got => 'oo', exists => 1, seen => {}, convert => sub { $_[0] })],
            [{ got => 'oo' }, DNE],
            "1 delta, one failed check"
        );

        like(
            [$one->deltas(got => 'fox', exists => 1, seen => {}, convert => sub { $_[0] })],
            [{ got => 'fox' }, { got => 'fox' }, DNE],
            "2 deltas, one per failed check"
        );

        $after_each->();
    };
};

done_testing;
