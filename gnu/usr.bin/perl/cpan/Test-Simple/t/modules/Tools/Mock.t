use Test2::Bundle::Extended -target => 'Test2::Compare::Custom';

use Test2::Tools::Mock qw{
    mock_obj mock_class
    mock_do  mock_build
    mock_accessor mock_accessors
    mock_getter   mock_getters
    mock_setter   mock_setters
    mock_building
};

use Scalar::Util qw/reftype blessed/;

imported_ok qw{
    mock_obj mock_class
    mock_do  mock_build
    mock_accessor mock_accessors
    mock_getter   mock_getters
    mock_setter   mock_setters
    mock_building
};

subtest generators => sub {
    # These are all thin wrappers around HashBase subs, we just test that we
    # get subs, HashBase subtest that the thing we are wrapping produce the
    # correct type of subs.

    my %accessors = mock_accessors qw/foo bar baz/;
    is([sort keys %accessors], [sort qw/foo bar baz/], "All 3 keys set");
    is(reftype($accessors{$_}), 'CODE', "sub as value for $_") for qw/foo bar baz/;

    is(reftype(mock_accessor('xxx')), 'CODE', "Generated an accessor");

    my %getters = mock_getters 'get_' => qw/foo bar baz/;
    is([sort keys %getters], [sort qw/get_foo get_bar get_baz/], "All 3 keys set");
    is(reftype($getters{"get_$_"}), 'CODE', "sub as value for get_$_") for qw/foo bar baz/;

    is(reftype(mock_getter('xxx')), 'CODE', "Generated a getter");

    my %setters = mock_setters 'set_' => qw/foo bar baz/;
    is([sort keys %setters], [sort qw/set_foo set_bar set_baz/], "All 3 keys set");
    is(reftype($setters{"set_$_"}), 'CODE', "sub as value for set_$_") for qw/foo bar baz/;

    is(reftype(mock_setter('xxx')), 'CODE', "Generated a setter");
};

subtest mocks => sub {
    my $inst;
    my $control;
    my $class;

    my $object = sub {
        $inst      = mock_obj({}, add_constructor => [new => 'hash']);
        ($control) = mocked($inst);
        $class     = $control->class;
    };

    my $package = sub {
        $control = mock_class('Fake::Class', add_constructor => [new => 'hash']);
        $class   = $control->class;
        $inst    = $class->new;
    };

    for my $case ($object, $package) {
        $case->();

        isa_ok($control, 'Test2::Mock');
        isa_ok($inst, $class);
        ok($class, "got a class");

        subtest mocked => sub {
            ok(!mocked('main'), "main class is not mocked");
            is(mocked($inst), 1, "Only 1 control object for this instance");
            my ($c) = mocked($inst);
            ref_is($c, $control, "got correct control when checking if an object was mocked");

            my $control2 = mock_class($control->class);

            is(mocked($inst), 2, "now 2 control objects for this instance");
            my ($c1, $c2) = mocked($inst);
            ref_is($c1, $control, "got first control");
            ref_is($c2, $control2, "got second control");
        };

        subtest build_and_do => sub {
            like(
                dies { mock_build(undef, sub { 1 }) },
                qr/mock_build requires a Test2::Mock object as its first argument/,
                "control is required",
            );

            like(
                dies { mock_build($control, undef) },
                qr/mock_build requires a coderef as its second argument/,
                "Must have a coderef to build"
            );

            like(
                dies { mock_do add => (foo => sub { 'foo' }) },
                qr/Not currently building a mock/,
                "mock_do outside of a build fails"
            );

            ok(!mock_building, "no mock is building");
            my $ran = 0;
            mock_build $control => sub {
                is(mock_building, $control, "Building expected control");

                like(
                    dies { mock_do 'foo' => 1 },
                    qr/'foo' is not a valid action for mock_do\(\)/,
                    "invalid action"
                );

                mock_do add => (
                    foo => sub { 'foo' },
                );

                can_ok($inst, 'foo');
                is($inst->foo, 'foo', "added sub");

                $ran++;
            };

            ok(!mock_building, "no mock is building");
            ok($ran, "build sub completed successfully");
        };
    }
};

subtest mock_obj => sub {
    my $ref = {};
    my $obj = mock_obj $ref;
    is($ref, $obj, "blessed \$ref");
    is($ref->foo(1), 1, "is vivifying object");

    my $ran = 0;
    $obj = mock_obj(sub { $ran++ });
    is($ref->foo(1), 1, "is vivifying object");
    is($ran, 1, "code ran");

    $obj = mock_obj { foo => 'foo' } => (
        add => [ bar => sub { 'bar' }],
    );

    # We need to test the methods returned by ->can before we call the subs by
    # name. This lets us be sure that this works _before_ the AUTOLOAD
    # actually creates the named sub for real.
    my $foo = $obj->can('foo');
    $obj->$foo('foo2');
    is($obj->$foo, 'foo2', "->can('foo') returns a method that works as a setter");
    $obj->$foo('foo');

    my $bar = $obj->can('bar');
    is($obj->$bar, 'bar', "->can('bar') returns a method");
    ok(!$obj->can('baz'), "mock object ->can returns false for baz");

    is($obj->foo, 'foo', "got value for foo");
    is($obj->bar, 'bar', "got value for bar");

    ok($obj->can('foo'), "mock object ->can returns true for foo");
    ok($obj->can('bar'), "mock object ->can returns true for bar");
    ok($obj->can('isa'), "mock object ->can returns true for isa");

    $foo = $obj->can('foo');

    my ($c) = mocked($obj);
    ok($c, "got control");
    is($obj->{'~~MOCK~CONTROL~~'}, $c, "control is stashed");

    my $class = $c->class;
    my $file = $c->file;
    ok($INC{$file}, "Mocked Loaded");

    $obj = undef;
    $c = undef;

    ok(!$INC{$file}, "Not loaded anymore");
};

subtest mock_class_basic => sub {
    my $c = mock_class 'Fake';
    isa_ok($c, 'Test2::Mock');
    is($c->class, 'Fake', "Control for 'Fake'");
    $c = undef;

    # Check with an instance
    my $i = bless {}, 'Fake';
    $c = mock_class $i;
    isa_ok($c, 'Test2::Mock');
    is($c->class, 'Fake', "Control for 'Fake'");

    is([mocked($i)], [$c], "is mocked");
};

subtest post => sub {
    ok(!"Fake$_"->can('check'), "mock $_ did not leak") for 1 .. 5;
};

ok(!"Fake$_"->can('check'), "mock $_ did not leak") for 1 .. 5;

subtest just_mock => sub {
    like(
        dies { mock undef },
        qr/undef is not a valid first argument to mock/,
        "Cannot mock undef"
    );

    like(
        dies { mock 'fakemethodname' },
        qr/'fakemethodname' does not look like a package name, and is not a valid control method/,
        "invalid mock arg"
    );

    my $c = mock 'Fake';
    isa_ok($c, 'Test2::Mock');
    is($c->class, 'Fake', "mocked correct class");
    mock $c => sub {
        mock add => (foo => sub { 'foo' });
    };

    can_ok('Fake', 'foo');
    is(Fake->foo(), 'foo', "mocked build, mocked do");

    my $o = mock;
    ok(blessed($o), "created object");
    $c = mocked($o);
    ok($c, "got control");

    $o = mock { foo => 'foo' };
    is($o->foo, 'foo', "got the expected result");
    is($o->{foo}, 'foo', "blessed the reference");

    $c = mock $o;
    isa_ok($o, $c->class);


    my $code = mock accessor => 'foo';
    ok(reftype($code), 'CODE', "Generated an accessor");
};

subtest handlers => sub {
    Test2::Tools::Mock->add_handler(__PACKAGE__,
        sub {
            is(
                {@_},
                {
                    class   => 'Foo',
                    caller  => T(),
                    builder => T(),
                    args    => T(),
                }
            );
            1;
        }
    );

    is(
        dies {
            mock Foo => {add => ['xxx' => sub { 'xxx' }]}
        },
        undef,
        "did not die"
    );
};

subtest set => sub {
    package My::Set;
    sub foo { 'foo' }

    package main;

    my $mock = mock 'My::Set' => (
        set => [
            foo => sub { 'FOO' },
            bar => sub { 'BAR' },
        ],
    );

    is(My::Set->foo, 'FOO', "overrode 'foo'");
    is(My::Set->bar, 'BAR', "injected 'bar'");
};

done_testing;
