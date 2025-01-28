use Test2::Bundle::Extended -target => 'Test2::Mock';
use Test2::API qw/context/;

use Scalar::Util qw/blessed/;

# If we reuse the same package name (Fake) over and over we can end up
# triggering some weird Perl core issues. With Perl 5.14 and 5.16 we were
# seeing "panic: gp_free failed to free glob pointer - something is repeatedly
# re-creating entries at ..."
#
# So instead we use Fake, Fake2, Fake3, etc. It's not very elegant, but it
# gets the job done.

subtest construction => sub {
    my %calls;
    my $c = Test2::Mock->new(
        class => 'Test2::Mock',
        before => [ class => sub { $calls{class}++ } ],
        override => [
            parent => sub { $calls{parent}++ },
            child  => sub { $calls{child}++  },
        ],
        add => [
            foo => sub { $calls{foo}++ },
        ],
    );

    my $one = Test2::Mock->new(
        class  => 'Fake',
        parent => 'Fake',
        child  => 'Fake',
        foo    => 'Fake',
    );
    isa_ok($one, 'Test2::Mock');

    is(
        \%calls,
        { foo => 1 },
        "Only called foo, did not call class, parent or child"
    );

    $c->reset_all;

    my @args;
    $c->add(foo => sub { push @args => \@_ });

    $one = Test2::Mock->new(
        class => 'Fake',
        foo   => 'string',
        foo   => [qw/a list/],
        foo   => {a => 'hash'},
    );
    isa_ok($one, 'Test2::Mock');

    is(
        \@args,
        [
            [$one, 'string'],
            [$one, qw/a list/],
            [$one, qw/a hash/],
        ],
        "Called foo with proper args, called it multiple times"
    );

    like(
        dies { Test2::Mock->new },
        qr/The 'class' field is required/,
        "Must specify a class"
    );

    like(
        dies { Test2::Mock->new(class => 'Fake', foo => sub { 1 }) },
        qr/'CODE\(.*\)' is not a valid argument for 'foo'/,
        "Val must be sane"
    );
};

subtest check => sub {
    my $one = Test2::Mock->new(class => 'Fake1');

    ok(lives { $one->_check }, "did not die");

    $one->set_child(1);

    like(
        dies {$one->_check},
        qr/There is an active child controller, cannot proceed/,
        "Cannot use a controller when it has a child"
    );
};

subtest purge_on_destroy => sub {
    my $one = Test2::Mock->new(class => 'Fake2');

    ok(!$one->purge_on_destroy, "Not set by default");
    $one->purge_on_destroy(1);
    ok($one->purge_on_destroy, "Can set");
    $one->purge_on_destroy(0);
    ok(!$one->purge_on_destroy, "Can Unset");

    {
        # need to hide the glob assignment from the parser.
        no strict 'refs';
        *{"Fake2::foo"} = sub { 'foo' };
    }

    can_ok('Fake2', 'foo');
    $one = undef;
    can_ok('Fake2', 'foo'); # Not purged

    $one = Test2::Mock->new(class => 'Fake2');
    $one->purge_on_destroy(1);
    $one = undef;
    my $stash = do { no strict 'refs'; \%{"Fake2::"}; };
    ok(!keys %$stash, "no keys left in stash");
    ok(!Fake2->can('foo'), 'purged sub');
};

subtest stash => sub {
    my $one = Test2::Mock->new(class => 'Fake3');
    my $stash = $one->stash;

    ok($stash, "got a stash");
    is($stash, {}, "stash is empty right now");

    {
        # need to hide the glob assignment from the parser.
        no strict 'refs';
        *{"Fake3::foo"} = sub { 'foo' };
    }

    ok($stash->{foo}, "See the new sub in the stash");
    ok(*{$stash->{foo}}{CODE}, "Code slot is populated");
};

subtest file => sub {
    my $fake = Test2::Mock->new(class => 'Fake4');
    my $complex = Test2::Mock->new(class => "A::Fake'Module::With'Separators");

    is($fake->file, "Fake4.pm", "Got simple filename");

    is($complex->file, "A/Fake/Module/With/Separators.pm", "got complex filename");
};

subtest block_load => sub {
    my $one;

    my $construction = sub {
        $one = Test2::Mock->new(class => 'Fake5', block_load => 1);
    };

    my $post_construction = sub {
        $one = Test2::Mock->new(class => 'Fake5');
        $one->block_load;
    };

    for my $case ($construction, $post_construction) {
        $one = undef;
        ok(!$INC{'Fake5.pm'}, "Does not appear to be loaded yet");

        $case->();

        ok($INC{'Fake5.pm'}, '%INC is populated');

        $one = undef;
        ok(!$INC{'Fake5.pm'}, "Does not appear to be loaded anymore");
    }
};

subtest block_load_fail => sub {
    $INC{'Fake6.pm'} = 'path/to/Fake6.pm';

    my $one = Test2::Mock->new(class => 'Fake6');

    like(
        dies { $one->block_load },
        qr/Cannot block the loading of module 'Fake6', already loaded in file/,
        "Fails if file is already loaded"
    );
};

subtest constructors => sub {
    my $one = Test2::Mock->new(
        class => 'Fake7',
        add_constructor => [new => 'hash'],
    );

    can_ok('Fake7', 'new');

    my $i = Fake7->new(foo => 'bar');
    isa_ok($i, 'Fake7');
    is($i, { foo => 'bar' }, "Has params");

    $one->override_constructor(new => 'ref');

    my $ref = { 'foo' => 'baz' };
    $i = Fake7->new($ref);
    isa_ok($i, 'Fake7');
    is($i, { foo => 'baz' }, "Has params");
    is($i, $ref, "same reference");
    ok(blessed($ref), "blessed original ref");

    $one->override_constructor(new => 'ref_copy');
    $ref = { 'foo' => 'bat' };
    $i = Fake7->new($ref);
    isa_ok($i, 'Fake7');
    is($i, { foo => 'bat' }, "Has params");
    ok($i != $ref, "different reference");
    ok(!blessed($ref), "original ref is not blessed");

    $ref = [ 'foo', 'bar' ];
    $i = Fake7->new($ref);
    isa_ok($i, 'Fake7');
    is($i, [ 'foo', 'bar' ], "has the items");
    ok($i != $ref, "different reference");
    ok(!blessed($ref), "original ref is not blessed");

    like(
        dies { $one->override_constructor(new => 'bad') },
        qr/'bad' is not a known constructor type/,
        "Bad constructor type (override)"
    );

    like(
        dies { $one->add_constructor(uhg => 'bad') },
        qr/'bad' is not a known constructor type/,
        "Bad constructor type (add)"
    );

    $one->override_constructor(new => 'array');
    $one = Fake7->new('a', 'b');
    is($one, ['a', 'b'], "is an array");
    isa_ok($one, 'Fake7');
};

subtest autoload => sub {
    my $one = Test2::Mock->new(
        class => 'Fake8',
        add_constructor => [new => 'hash'],
    );

    my $i = Fake8->new;
    isa_ok($i, 'Fake8');

    ok(!$i->can('foo'), "Cannot do 'foo'");
    like(dies {$i->foo}, qr/Can't locate object method "foo" via package "Fake8"/, "Did not autload");

    $one->autoload;

    ok(lives { $i->foo }, "Created foo") || return;
    can_ok($i, 'foo'); # Added the sub to the package

    is($i->foo, undef, "no value");
    $i->foo('bar');
    is($i->foo, 'bar', "set value");
    $i->foo(undef);
    is($i->foo, undef, "unset value");

    ok(
        dies { $one->autoload },
        qr/Class 'Fake8' already has an AUTOLOAD/,
        "Cannot add additional autoloads"
    );

    $one->reset_all;

    ok(!$i->can('AUTOLOAD'), "AUTOLOAD removed");
    ok(!$i->can('foo'), "AUTOLOADed sub removed");

    $one->autoload;
    $i->foo;

    ok($i->can('AUTOLOAD'), "AUTOLOAD re-added");
    ok($i->can('foo'), "AUTOLOADed sub re-added");

    $one = undef;

    ok(!$i->can('AUTOLOAD'), "AUTOLOAD removed (destroy)");
    ok(!$i->can('foo'), "AUTOLOADed sub removed (destroy)");

    my $two = Test2::Mock->new(
        class => 'Fake88',
        add_constructor => [new => 'hash'],
        track => 1,
        autoload => 1,
    );

    my $j = Fake88->new;
    ok(lives { $j->foo }, "Created foo") || return;
    can_ok($j, 'foo'); # Added the sub to the package

    is(
        $two->sub_tracking,
        {foo => [{sub_name => 'foo', sub_ref => T, args => [exact_ref($j)]}]},
        "Tracked autoloaded sub (sub tracking)"
    );

    is(
        $two->call_tracking,
        [{sub_name => 'foo', sub_ref => T, args => [exact_ref($j)]}],
        "Tracked autoloaded sub (call tracking)"
    );

};

subtest autoload_failures => sub {
    my $one = Test2::Mock->new(class => 'fake');

    $one->add('AUTOLOAD' => sub { 1 });

    like(
        dies { $one->autoload },
        qr/Class 'fake' already has an AUTOLOAD/,
        "Cannot add autoload when there is already an autoload"
    );

    $one = undef;

    $one = Test2::Mock->new(class => 'bad package');
    like(
        dies { $one->autoload },
        qr/syntax error/,
        "Error inside the autoload eval"
    );
};

subtest ISA => sub {
    # This is to satisfy perl that My::Parent is loaded
    no warnings 'once';
    local *My::Parent::foo = sub { 'foo' };

    my $one = Test2::Mock->new(
        class => 'Fake9',
        add_constructor => [new => 'hash'],
        add => [
            -ISA => ['My::Parent'],
        ],
    );

    isa_ok('Fake9', 'My::Parent');
    is(Fake9->foo, 'foo', "Inherited sub from parent");
};

subtest before => sub {
    {
        # need to hide the glob assignment from the parser.
        no strict 'refs';
        *{"Fake10::foo"} = sub { 'foo' };
    }

    my $thing;

    my $one = Test2::Mock->new(class => 'Fake10');
    $one->before('foo' => sub { $thing = 'ran before foo' });

    ok(!$thing, "nothing ran yet");
    is(Fake10->foo, 'foo', "got expected return");
    is($thing, 'ran before foo', "ran the before");
};

subtest before => sub {
    my $want;
    {
        # need to hide the glob assignment from the parser.
        no strict 'refs';
        *{"Fake11::foo"} = sub {
            $want = wantarray;
            return qw/f o o/ if $want;
            return 'foo' if defined $want;
            return;
        };
    }

    my $ran = 0;

    my $one = Test2::Mock->new(class => 'Fake11');
    $one->after('foo' => sub { $ran++ });

    is($ran, 0, "nothing ran yet");

    is(Fake11->foo, 'foo', "got expected return (scalar)");
    is($ran, 1, "ran the before");
    ok(defined($want) && !$want, "scalar context");

    is([Fake11->foo], [qw/f o o/], "got expected return (list)");
    is($ran, 2, "ran the before");
    is($want, 1, "list context");

    Fake11->foo; # Void return
    is($ran, 3, "ran the before");
    is($want, undef, "void context");
};

subtest around => sub {
    my @things;
    {
        # need to hide the glob assignment from the parser.
        no strict 'refs';
        *{"Fake12::foo"} = sub {
            push @things => ['foo', \@_];
        };
    }

    my $one = Test2::Mock->new(class => 'Fake12');
    $one->around(foo => sub {
        my ($orig, @args) = @_;
        push @things => ['pre', \@args];
        $orig->('injected', @args);
        push @things => ['post', \@args];
    });

    Fake12->foo(qw/a b c/);

    is(
        \@things,
        [
            ['pre'  => [qw/Fake12 a b c/]],
            ['foo'  => [qw/injected Fake12 a b c/]],
            ['post' => [qw/Fake12 a b c/]],
        ],
        "Got all the things!"
    );
};

subtest 'add and current' => sub {
    my $one = Test2::Mock->new(
        class => 'Fake13',
        add_constructor => [new => 'hash'],
        add => [
            foo => { val => 'foo' },
            bar => 'rw',
            baz => { is => 'rw', field => '_baz' },
            -DATA => { my => 'data' },
            -DATA => [ qw/my data/ ],
            -DATA => sub { 'my data' },
            -DATA => \"data",
        ],
    );

    # Do some outside constructor to test both paths
    $one->add(
        reader => 'ro',
        writer => 'wo',
        -UHG   => \"UHG",
        rsub   => { val => sub { 'rsub' } },

        # Without $x the compiler gets smart and makes it always return the
        # same reference.
        nsub   => sub { my $x = ''; sub { $x . 'nsub' } },
    );

    can_ok('Fake13', qw/new foo bar baz DATA reader writer rsub nsub/);

    like(
        dies { $one->add(foo => sub { 'nope' }) },
        qr/Cannot add '&Fake13::foo', symbol is already defined/,
        "Cannot add a CODE symbol that is already defined"
    );

    like(
        dies { $one->add(-UHG => \'nope') },
        qr/Cannot add '\$Fake13::UHG', symbol is already defined/,
        "Cannot add a SCALAR symbol that is already defined"
    );

    my $i = Fake13->new();
    is($i->foo, 'foo', "by value");

    is($i->bar, undef, "Accessor not set");
    is($i->bar('bar'), 'bar', "Accessor setting");
    is($i->bar, 'bar', "Accessor was set");

    is($i->baz, undef, "no value yet");
    ok(!$i->{_bar}, "hash element is empty");
    is($i->baz('baz'), 'baz', "setting");
    is($i->{_baz}, 'baz', "set field");
    is($i->baz, 'baz', "got value");

    is($i->reader, undef, "No value for reader");
    is($i->reader('oops'), undef, "No value set");
    is($i->reader, undef, "Still No value for reader");
    is($i->{reader}, undef, 'element is empty');
    $i->{reader} = 'yay';
    is($i->{reader}, 'yay', 'element is set');

    is($i->{writer}, undef, "no value yet");
    $i->writer;
    is($i->{writer}, undef, "Set to undef");
    is($i->writer('xxx'), 'xxx', "Adding value");
    is($i->{writer}, 'xxx', "was set");
    is($i->writer, undef, "writer always writes");
    is($i->{writer}, undef, "Set to undef");

    is($i->rsub, $i->rsub, "rsub always returns the same ref");
    is($i->rsub->(), 'rsub', "ran rsub");

    ok($i->nsub != $i->nsub, "nsub returns a new ref each time");
    is($i->nsub->(), 'nsub', "ran nsub");

    is($i->DATA, 'my data', "direct sub assignment");
    # These need to be eval'd so the parser does not shortcut the glob references
    ok(eval <<'    EOT', "Ran glob checks") || diag "Error: $@";
        is($Fake13::UHG, 'UHG', "Set package scalar (UHG)");
        is($Fake13::DATA, 'data', "Set package scalar (DATA)");
        is(\%Fake13::DATA, { my => 'data' }, "Set package hash");
        is(\@Fake13::DATA, [ my => 'data' ], "Set package array");
        1;
    EOT

    is($one->current($_), $i->can($_), "current works for sub $_")
        for qw/new foo bar baz DATA reader writer rsub nsub/;

    is(${$one->current('$UHG')}, 'UHG', 'got current $UHG');
    is(${$one->current('$DATA')}, 'data', 'got current $DATA');
    is($one->current('&DATA'), $i->can('DATA'), 'got current &DATA');
    is($one->current('@DATA'), [qw/my data/], 'got current @DATA');
    is($one->current('%DATA'), {my => 'data'}, 'got current %DATA');

    $one = undef;

    ok(!Fake13->can($_), "Removed sub $_") for qw/new foo bar baz DATA reader writer rsub nsub/;

    $one = Test2::Mock->new(class => 'Fake13');

    # Scalars are tricky, skip em for now.
    is($one->current('&DATA'), undef, 'no current &DATA');
    is($one->current('@DATA'), undef, 'no current @DATA');
    is($one->current('%DATA'), undef, 'no current %DATA');
};

subtest 'override and orig' => sub {
    # Define things so we can override them
    eval <<'    EOT' || die $@;
        package Fake14;

        sub new { 'old' }

        sub foo { 'old' }
        sub bar { 'old' }
        sub baz { 'old' }

        sub DATA { 'old' }
        our $DATA = 'old';
        our %DATA = (old => 'old');
        our @DATA = ('old');

        our $UHG = 'old';

        sub reader { 'old' }
        sub writer { 'old' }
        sub rsub   { 'old' }
        sub nsub   { 'old' }
    EOT

    my $check_initial = sub {
        is(Fake14->$_, 'old', "$_ is not overridden") for qw/new foo bar baz DATA reader writer rsub nsub/;
        ok(eval <<'        EOT', "Ran glob checks") || diag "Error: $@";
            is($Fake14::UHG,  'old',  'old package scalar (UHG)');
            is($Fake14::DATA, 'old', "Old package scalar (DATA)");
            is(\%Fake14::DATA, {old => 'old'}, "Old package hash");
            is(\@Fake14::DATA, ['old'], "Old package array");
            1;
        EOT
    };

    $check_initial->();

    my $one = Test2::Mock->new(
        class => 'Fake14',
        override_constructor => [new => 'hash'],
        override => [
            foo => { val => 'foo' },
            bar => 'rw',
            baz => { is => 'rw', field => '_baz' },
            -DATA => { my => 'data' },
            -DATA => [ qw/my data/ ],
            -DATA => sub { 'my data' },
            -DATA => \"data",
        ],
    );

    # Do some outside constructor to test both paths
    $one->override(
        reader => 'ro',
        writer => 'wo',
        -UHG   => \"UHG",
        rsub   => { val => sub { 'rsub' } },

        # Without $x the compiler gets smart and makes it always return the
        # same reference.
        nsub   => sub { my $x = ''; sub { $x . 'nsub' } },
    );

    like(
        dies { $one->override(nuthin => sub { 'nope' }) },
        qr/Cannot override '&Fake14::nuthin', symbol is not already defined/,
        "Cannot override a CODE symbol that is not defined"
    );

    like(
        dies { $one->override(-nuthin2 => \'nope') },
        qr/Cannot override '\$Fake14::nuthin2', symbol is not already defined/,
        "Cannot override a SCALAR symbol that is not defined"
    );

    my $i = Fake14->new();
    is($i->foo, 'foo', "by value");

    is($i->bar, undef, "Accessor not set");
    is($i->bar('bar'), 'bar', "Accessor setting");
    is($i->bar, 'bar', "Accessor was set");

    is($i->baz, undef, "no value yet");
    ok(!$i->{_bar}, "hash element is empty");
    is($i->baz('baz'), 'baz', "setting");
    is($i->{_baz}, 'baz', "set field");
    is($i->baz, 'baz', "got value");

    is($i->reader, undef, "No value for reader");
    is($i->reader('oops'), undef, "No value set");
    is($i->reader, undef, "Still No value for reader");
    is($i->{reader}, undef, 'element is empty');
    $i->{reader} = 'yay';
    is($i->{reader}, 'yay', 'element is set');

    is($i->{writer}, undef, "no value yet");
    $i->writer;
    is($i->{writer}, undef, "Set to undef");
    is($i->writer('xxx'), 'xxx', "Adding value");
    is($i->{writer}, 'xxx', "was set");
    is($i->writer, undef, "writer always writes");
    is($i->{writer}, undef, "Set to undef");

    is($i->rsub, $i->rsub, "rsub always returns the same ref");
    is($i->rsub->(), 'rsub', "ran rsub");

    ok($i->nsub != $i->nsub, "nsub returns a new ref each time");
    is($i->nsub->(), 'nsub', "ran nsub");

    is($i->DATA, 'my data', "direct sub assignment");
    # These need to be eval'd so the parser does not shortcut the glob references
    ok(eval <<'    EOT', "Ran glob checks") || diag "Error: $@";
        is($Fake14::UHG, 'UHG', "Set package scalar (UHG)");
        is($Fake14::DATA, 'data', "Set package scalar (DATA)");
        is(\%Fake14::DATA, { my => 'data' }, "Set package hash");
        is(\@Fake14::DATA, [ my => 'data' ], "Set package array");
        1;
    EOT

    is($one->current($_), $i->can($_), "current works for sub $_")
        for qw/new foo bar baz DATA reader writer rsub nsub/;

    is(${$one->current('$UHG')}, 'UHG', 'got current $UHG');
    is(${$one->current('$DATA')}, 'data', 'got current $DATA');
    is($one->current('&DATA'), $i->can('DATA'), 'got current &DATA');
    is($one->current('@DATA'), [qw/my data/], 'got current @DATA');
    is($one->current('%DATA'), {my => 'data'}, 'got current %DATA');

    is($one->orig($_)->(), 'old', "got original $_") for qw/new foo bar baz DATA reader writer rsub nsub/;

    is(${$one->orig('$UHG')},  'old',  'old package scalar (UHG)');
    is(${$one->orig('$DATA')}, 'old', "Old package scalar (DATA)");
    is($one->orig('%DATA'), {old => 'old'}, "Old package hash");
    is($one->orig('@DATA'), ['old'], "Old package array");

    like(
        dies { $one->orig('not_mocked') },
        qr/Symbol '&not_mocked' is not mocked/,
        "Cannot get original for something not mocked"
    );

    like(
        dies { Test2::Mock->new(class => 'AnotherFake14')->orig('no_mocks') },
        qr/No symbols have been mocked yet/,
        "Cannot get original when nothing is mocked"
    );

    $one = undef;

    $check_initial->();
};

subtest restore_reset => sub {
    my $one = Test2::Mock->new( class => 'Fake15' );

    $one->add(foo => sub { 'a' });
    $one->add(-foo => \'a');
    $one->add(-foo => ['a']);

    $one->override(foo => sub { 'b' });
    $one->override(foo => sub { 'c' });
    $one->override(foo => sub { 'd' });
    $one->override(foo => sub { 'e' });

    is(Fake15->foo, 'e', "latest override");
    is(eval '$Fake15::foo', 'a', "scalar override remains");
    is(eval '\@Fake15::foo', ['a'], "array override remains");

    $one->restore('foo');
    is(Fake15->foo, 'd', "second latest override");
    is(eval '$Fake15::foo', 'a', "scalar override remains");
    is(eval '\@Fake15::foo', ['a'], "array override remains");

    $one->restore('foo');
    is(Fake15->foo, 'c', "second latest override");
    is(eval '$Fake15::foo', 'a', "scalar override remains");
    is(eval '\@Fake15::foo', ['a'], "array override remains");

    $one->reset('foo');
    ok(!Fake15->can('foo'), "no more override");
    is(eval '$Fake15::foo', 'a', "scalar override remains");
    is(eval '\@Fake15::foo', ['a'], "array override remains");

    $one->add(foo => sub { 'a' });
    is(Fake15->foo, 'a', "override");

    $one->reset_all;
    ok(!Fake15->can('foo'), "no more override");
    is(eval '$Fake15::foo', undef, "scalar override removed");

    no strict 'refs';
    ok(!*{'Fake15::foo'}{ARRAY}, "array override removed");
};

subtest exceptions => sub {
    my $one = Test2::Mock->new( class => 'Fake16' );
    like(
        dies { $one->new(class => 'AnotherFake16') },
        qr/Called new\(\) on a blessed instance, did you mean to call \$control->class->new\(\)\?/,
        "Cannot call new on a blessed instance"
    );

    like(
        dies { Test2::Mock->new(class => 'AnotherFake16', foo => 1) },
        qr/'foo' is not a valid constructor argument for Test2::Mock/,
        "Validate constructor args"
    );

    like(
        dies { Test2::Mock->new(class => 'AnotherFake16', override_constructor => ['xxx', 'xxx']) },
        qr/'xxx' is not a known constructor type/,
        "Invalid constructor type"
    );

    like(
        dies { Test2::Mock->new(class => 'AnotherFake16', add_constructor => ['xxx', 'xxx']) },
        qr/'xxx' is not a known constructor type/,
        "Invalid constructor type"
    );

    like(
        dies { $one->orig('foo') },
        qr/No symbols have been mocked yet/,
        "No symbols are mocked yet"
    );

    like(
        dies { $one->restore('foo') },
        qr/No symbols are mocked/,
        "No symbols yet!"
    );

    like(
        dies { $one->reset('foo') },
        qr/No symbols are mocked/,
        "No symbols yet!"
    );

    $one->add(xxx => sub { 1 });
    like(
        dies { $one->orig('foo') },
        qr/Symbol '&foo' is not mocked/,
        "did not mock foo"
    );
    like(
        dies { $one->restore('foo') },
        qr/Symbol '&foo' is not mocked/,
        "did not mock foo"
    );
    like(
        dies { $one->reset('foo') },
        qr/Symbol '&foo' is not mocked/,
        "did not mock foo"
    );

    my $bare = Test2::Mock->new(
        class => 'Fake17',
        autoload => 1,
    );

    like(
        dies { $bare->override( missing => 1 ) },
        qr/Cannot override '&Fake17::missing', symbol is not already defined/,
        "Cannot override a method that is not defined in an AUTOLOAD mock"
    );
};

subtest override_inherited_method => sub {
    package ABC;
    our @ISA = 'DEF';

    package DEF;

    sub foo { 'foo' };

    package main;
    is(ABC->foo, 'foo', "Original");

    my $mock = Test2::Mock->new(class => 'ABC');
    $mock->override('foo' => sub { 'bar' });
    is(ABC->foo, 'bar', "Overrode method from base class");

    $mock->reset('foo');
    $mock->add('foo' => sub { 'baz' });
    is(ABC->foo, 'baz', "Added method");
};

subtest set => sub {
    package My::Set;
    sub foo { 'foo' }

    package main;

    my $mock = Test2::Mock->new(class => 'My::Set');
    $mock->set(foo => sub { 'FOO' });
    $mock->set(bar => sub { 'BAR' });

    is(My::Set->foo, 'FOO', "overrode 'foo'");
    is(My::Set->bar, 'BAR', "injected 'bar'");
};

subtest tracking => sub {
    package My::Track;
    sub foo { 'foo' }

    package main;

    my $mock = Test2::Mock->new(class => 'My::Track', track => 1);
    my $FOO = sub { 'FOO' };
    my $BAR = sub { 'BAR' };
    $mock->set(foo => $FOO);
    $mock->set(bar => $BAR);

    is(My::Track->foo(1,2), 'FOO', "overrode 'foo'");
    is(My::Track->bar(3,4), 'BAR', "injected 'bar'");

    is(
        $mock->sub_tracking,
        {
            foo => [{sub_name => 'foo', sub_ref => exact_ref($FOO), args => ['My::Track', 1, 2]}],
            bar => [{sub_name => 'bar', sub_ref => exact_ref($BAR), args => ['My::Track', 3, 4]}],
        },
        "Tracked both initial calls (sub)"
    );
    is(
        $mock->call_tracking,
        [
            {sub_name => 'foo', sub_ref => exact_ref($FOO), args => ['My::Track', 1, 2]},
            {sub_name => 'bar', sub_ref => exact_ref($BAR), args => ['My::Track', 3, 4]}
        ],
        "Tracked both initial calls (call)"
    );

    My::Track->foo(5, 6);
    is(
        $mock->sub_tracking,
        {
            foo => [
                {sub_name => 'foo', sub_ref => exact_ref($FOO), args => ['My::Track', 1, 2]},
                {sub_name => 'foo', sub_ref => exact_ref($FOO), args => ['My::Track', 5, 6]},
            ],
            bar => [{sub_name => 'bar', sub_ref => exact_ref($BAR), args => ['My::Track', 3, 4]}],
        },
        "Tracked new call (sub)"
    );
    is(
        $mock->call_tracking,
        [
            {sub_name => 'foo', sub_ref => exact_ref($FOO), args => ['My::Track', 1, 2]},
            {sub_name => 'bar', sub_ref => exact_ref($BAR), args => ['My::Track', 3, 4]},
            {sub_name => 'foo', sub_ref => exact_ref($FOO), args => ['My::Track', 5, 6]},
        ],
        "Tracked new call (call)"
    );


    $mock->clear_sub_tracking('xxx', 'foo');
    My::Track->foo(7, 8);
    is(
        $mock->sub_tracking,
        {
            foo => [{sub_name => 'foo', sub_ref => exact_ref($FOO), args => ['My::Track', 7, 8]}],
            bar => [{sub_name => 'bar', sub_ref => exact_ref($BAR), args => ['My::Track', 3, 4]}],
        },
        "Cleared specific sub, Tracked new call (sub)"
    );
    is(
        $mock->call_tracking,
        [
            {sub_name => 'foo', sub_ref => exact_ref($FOO), args => ['My::Track', 1, 2]},
            {sub_name => 'bar', sub_ref => exact_ref($BAR), args => ['My::Track', 3, 4]},
            {sub_name => 'foo', sub_ref => exact_ref($FOO), args => ['My::Track', 5, 6]},
            {sub_name => 'foo', sub_ref => exact_ref($FOO), args => ['My::Track', 7, 8]},
        ],
        "did not clear call tracking"
    );

    $mock->clear_sub_tracking();
    is($mock->sub_tracking, {}, "Cleared all sub tracking");

    $mock->clear_call_tracking();
    is($mock->call_tracking, [], "Cleared call tracking");

    My::Track->foo(9, 10);
    is(
        $mock->sub_tracking,
        {
            foo => [{sub_name => 'foo', sub_ref => exact_ref($FOO), args => ['My::Track', 9, 10]}],
        },
        "Tracked new call (sub)"
    );
    is(
        $mock->call_tracking,
        [
            {sub_name => 'foo', sub_ref => exact_ref($FOO), args => ['My::Track', 9, 10]},
        ],
        "Tracked new call (call)"
    );

    $mock = undef;

    is(My::Track->foo, 'foo', "Original restored");
};

subtest prototypes => sub {
    sub foo_022 ($) { $_[0] }    # Because this is test 22.

    # NOTE that we make use of the prototype in the following code.

    is(foo_022 'bar', 'bar', 'foo_022 returns its argument');

    my $one = Test2::Mock->new(class => __PACKAGE__);

    my $warning = warnings {
        $one->before(foo_022 => sub ($) { warn "Before foo_022( '$_[0]' )" });
        is(foo_022 'baz', 'baz', 'foo_022 still returns its argument');
    };
    is(
        $warning, [
            match qr/\ABefore foo_022\( 'baz' \)/,
        ],
        'Got warning from before() hook'
    );
    $one->reset_all();

    $warning = warnings {
        is(foo_022 'foo', 'foo', 'foo_022 persists in returning its argument');
    };
    is $warning, [], 'No warnings after resetting mock';

    $warning = warnings {
        $one->after(foo_022 => sub ($) { warn "After foo_022( '$_[0]' )" });
        is(foo_022 'plugh', 'plugh', 'foo_022 steadfastly returns its argument');
    };
    is(
        $warning, [
            match qr/\AAfter foo_022\( 'plugh' \)/,
        ],
        'Got warning from after() hook'
    );
    $one->reset_all();

    $warning = warnings {
        $one->around(foo_022 => sub ($) { return $_[0]->($_[1]) x 2 });
        is foo_022 '42', '4242', 'With around(), foo_022 now doubles its return';
    };
    is($warning, [], 'around() produced no warnings');
};

done_testing;
