use Test2::Bundle::Extended -target => 'Test2::Tools::Class';

{
    package Temp;
    use Test2::Tools::Class;

    main::imported_ok(qw/can_ok isa_ok DOES_ok/);
}

{
    package X;

    sub can {
        my $thing = pop;
        return 1 if $thing =~ m/x/;
        return 1 if $thing eq 'DOES';
    }

    sub isa {
        my $thing = pop;
        return 1 if $thing =~ m/x/;
    }

    sub DOES {
        my $thing = pop;
        return 1 if $thing =~ m/x/;
    }
}

{
    package XYZ;
    use Carp qw/croak/;
    sub isa { croak 'oops' };
    sub can { croak 'oops' };
    sub DOES { croak 'oops' };
}

{
    package My::String;
    use overload '""' => sub { "xxx\nyyy" };

    sub DOES { 0 }
}

like(
    intercept {
        my $str = bless {}, 'My::String';

        isa_ok('X', qw/axe box fox/);
        can_ok('X', qw/axe box fox/);
        DOES_ok('X', qw/axe box fox/);
        isa_ok($str, 'My::String');

        isa_ok('X',  qw/foo bar axe box/);
        can_ok('X',  qw/foo bar axe box/);
        DOES_ok('X', qw/foo bar axe box/);

        isa_ok($str, 'X');
        can_ok($str, 'X');
        DOES_ok($str, 'X');

        isa_ok(undef, 'X');
        isa_ok('', 'X');
        isa_ok({}, 'X');

        isa_ok('X',  [qw/axe box fox/], 'alt name');
        can_ok('X',  [qw/axe box fox/], 'alt name');
        DOES_ok('X', [qw/axe box fox/], 'alt name');

        isa_ok('X',  [qw/foo bar axe box/], 'alt name');
        can_ok('X',  [qw/foo bar axe box/], 'alt name');
        DOES_ok('X', [qw/foo bar axe box/], 'alt name');
    },
    array {
        event Ok => { pass => 1, name => 'X->isa(...)' };
        event Ok => { pass => 1, name => 'X->can(...)' };
        event Ok => { pass => 1, name => 'X->DOES(...)' };
        event Ok => { pass => 1, name => qr/My::String=.*->isa\('My::String'\)/ };

        fail_events Ok => sub { call pass => 0 };
        event Diag => {message => "Failed: X->isa('foo')"};
        event Diag => {message => "Failed: X->isa('bar')"};
        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => "Failed: X->can('foo')" };
        event Diag => { message => "Failed: X->can('bar')" };
        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => "Failed: X->DOES('foo')" };
        event Diag => { message => "Failed: X->DOES('bar')" };

        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => qr/Failed: My::String=HASH->isa\('X'\)/ };
        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => qr/Failed: My::String=HASH->can\('X'\)/ };
        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => qr/Failed: My::String=HASH->DOES\('X'\)/ };

        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => qr/<undef> is neither a blessed reference or a package name/ };
        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => qr/'' is neither a blessed reference or a package name/ };
        fail_events Ok => sub { call pass => 0 };
        event Diag => { message => qr/HASH is neither a blessed reference or a package name/ };

        event Ok => { pass => 1, name => 'alt name' };
        event Ok => { pass => 1, name => 'alt name' };
        event Ok => { pass => 1, name => 'alt name' };

        fail_events Ok => sub { call pass => 0; call name => 'alt name' };
        event Diag => {message => "Failed: X->isa('foo')"};
        event Diag => {message => "Failed: X->isa('bar')"};
        fail_events Ok => sub { call pass => 0; call name => 'alt name' };
        event Diag => {message => "Failed: X->can('foo')"};
        event Diag => {message => "Failed: X->can('bar')"};
        fail_events Ok => sub { call pass => 0; call name => 'alt name' };
        event Diag => {message => "Failed: X->DOES('foo')"};
        event Diag => {message => "Failed: X->DOES('bar')"};

        end;
    },
    "'can/isa/DOES_ok' events"
);

my $override = UNIVERSAL->can('DOES') ? 1 : 0;
note "Will override UNIVERSAL::can to hide 'DOES'" if $override;

my $events = intercept {
    my $can = \&UNIVERSAL::can;

    # If the platform does support 'DOES' lets pretend it doesn't.
    no warnings 'redefine';
    local *UNIVERSAL::can = sub {
        my ($thing, $sub) = @_;
        return undef if $sub eq 'DOES';
        $thing->$can($sub);
    } if $override;

    DOES_ok('A::Fake::Package', 'xxx');
};

like(
    $events,
    array {
        event Skip => {
            pass   => 1,
            name   => "A::Fake::Package->DOES('xxx')",
            reason => "'DOES' is not supported on this platform",
        };
    },
    "Test us skipped when platform does not support 'DOES'"
);

done_testing;
