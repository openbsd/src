use Test2::Bundle::Extended -target => 'Test2::Tools::ClassicCompare';

BEGIN { $ENV{TABLE_TERM_SIZE} = 80 }

use Test2::Util::Stash qw/purge_symbol/;
BEGIN {
    purge_symbol('&is');
    purge_symbol('&like');
    purge_symbol('&unlike');
    purge_symbol('&isnt');
    purge_symbol('&cmp_ok');

    not_imported_ok(qw/is is_deeply like unlike isnt cmp_ok/);
}

use Test2::Tools::ClassicCompare;

imported_ok(qw/is is_deeply like cmp_ok unlike isnt/);

my $ref = {};

is(undef, undef, "undef is undef");

is("foo", "foo", 'foo check');
is($ref,   "$ref", "flat check, ref as string right");
is("$ref", $ref,   "flat check, ref as string left");

isnt("bar", "foo", 'not foo check');
isnt({},   "$ref", "negated flat check, ref as string right");
isnt("$ref", {},   "negated flat check, ref as string left");

like('aaa', qr/a/, "have an a");
like('aaa', 'a', "have an a, not really a regex");

unlike('bbb', qr/a/, "do not have an a");
unlike('bbb', 'a', "do not have an a, not really a regex");

# Failures
my $events = intercept {
    def ok => (!is('foo', undef, "undef check"),     "undef check");
    def ok => (!is(undef, 'foo',   "undef check"),     "undef check");
    def ok => (!is('foo', 'bar', "string mismatch"), "string mismatch");
    def ok => (!isnt('foo', 'foo', "undesired match"), "undesired match");
    def ok => (!like('foo', qr/a/, "no match"), "no match");
    def ok => (!unlike('foo', qr/o/, "unexpected match"), "unexpected match");
};

do_def;

is_deeply(
    $events,
    array {
        filter_items { grep { !$_->isa('Test2::Event::Diag') } @_ };
        event Fail => { };
        event Fail => { };
        event Fail => { };
        event Fail => { };
        event Fail => { };
        event Fail => { };
        end;
    },
    "got failure events"
);

# is_deeply uses the same algorithm as the 'Compare' plugin, so it is already
# tested over there.
is_deeply(
    {foo => 1, bar => 'baz'},
    {foo => 1, bar => 'baz'},
    "Deep compare"
);

{
    package Foo;
    use overload '""' => sub { 'xxx' };
}
my $foo = bless({}, 'Foo');
like($foo, qr/xxx/, "overload");

my $thing = bless {}, 'Foo::Bar';

# Test cmp_ok in a separate package so we have access to the better tools.
package main2;

use Test2::Bundle::Extended;
BEGIN { main::purge_symbol('&cmp_ok') }
use Test2::Tools::ClassicCompare qw/cmp_ok/;
use Test2::Util::Table();
sub table { join "\n" => Test2::Util::Table::table(@_) }
use Test2::Util::Ref qw/render_ref/;

cmp_ok('x', 'eq', 'x', 'string pass');
cmp_ok(5, '==', 5, 'number pass');
cmp_ok(5, '==', 5.0, 'float pass');

my $file = __FILE__;
my $line = __LINE__ + 2;
like(
    warnings { cmp_ok(undef, '==', undef, 'undef pass') },
    [
        qr/uninitialized value.*at \(eval in cmp_ok\) \Q$file\E line $line/,
    ],
    "got expected warnings (number)"
);

$line = __LINE__ + 2;
like(
    warnings { cmp_ok(undef, 'eq', undef, 'undef pass') },
    [
        qr/uninitialized value.*at \(eval in cmp_ok\) \Q$file\E line $line/,
    ],
    "got expected warnings (string)"
);

like(
    intercept { cmp_ok('x', 'ne', 'x', 'string fail', 'extra diag') },
    array {
        fail_events Ok => sub {
            call pass => 0;
            call name => 'string fail';
        };
        event Diag => sub {
            call message => table(
                header => [qw/GOT OP CHECK/],
                rows   => [
                    [qw/x ne x/],
                ],
            );
        };
        event Diag => { message => 'extra diag' };
        end;
    },
    "Got 1 string fail event"
);

like(
    intercept { cmp_ok(5, '==', 42, 'number fail', 'extra diag') },
    array {
        fail_events Ok => sub {
            call pass => 0;
            call name => 'number fail';
        };
        event Diag => sub {
            call message => table(
                header => [qw/GOT OP CHECK/],
                rows   => [
                    [qw/5 == 42/],
                ],
            );
        };
        event Diag => { message => 'extra diag' };

        end;
    },
    "Got 1 number fail event"
);

my $warning;
$line = __LINE__ + 2;
like(
    intercept { $warning = main::warning { cmp_ok(5, '&& die', 42, 'number fail', 'extra diag') } },
    array {
        event Exception => { error => qr/42 at \(eval in cmp_ok\) \Q$file\E line $line/ };
        fail_events Ok => sub {
            call pass => 0;
            call name => 'number fail';
        };

        event Diag => sub {
            call message => table(
                header => [qw/GOT OP CHECK/],
                rows   => [
                    ['5', '&& die', '<EXCEPTION>'],
                ],
            );
        };
        event Diag => { message => 'extra diag' };

        end;
    },
    "Got exception in test"
);
like(
    $warning,
    qr/operator '&& die' is not supported \(you can add it to %Test2::Tools::ClassicCompare::OPS\)/,
    "Got warning about unsupported operator"
);

{
    package Overloaded::Foo42;
    use overload
        'fallback' => 1,
        '0+' => sub { 42    },
        '""' => sub { 'foo' };
}

$foo = bless {}, 'Overloaded::Foo42';

cmp_ok($foo, '==', 42, "numeric compare with overloading");
cmp_ok($foo, 'eq', 'foo', "string compare with overloading");

like(
    intercept {
        local $ENV{TS_TERM_SIZE} = 10000;
        cmp_ok($foo, 'ne', $foo, 'string fail', 'extra diag')
    },
    array {
        fail_events Ok => sub {
            call pass => 0;
            call name => 'string fail';
        };

        event Diag => sub {
            call message => table(
                header => [qw/TYPE GOT OP CHECK/],
                rows   => [
                    ['str', 'foo', 'ne', 'foo'],
                    ['orig', render_ref($foo), '', render_ref($foo)],
                ],
            );
        };
        event Diag => { message => 'extra diag' };

        end;
    },
    "Failed string compare, overload"
);

like(
    intercept {
        local $ENV{TS_TERM_SIZE} = 10000;
        cmp_ok($foo, '!=', $foo, 'number fail', 'extra diag')
    },
    array {
        fail_events Ok => sub {
            call pass => 0;
            call name => 'number fail';
        };

        event Diag => sub {
            call message => table(
                header => [qw/TYPE GOT OP CHECK/],
                rows   => [
                    ['num', '42', '!=', '42'],
                    ['orig', render_ref($foo), '', render_ref($foo)],
                ],
            );
        };
        event Diag => { message => 'extra diag' };

        end;
    },
    "Failed number compare, overload"
);

$line = __LINE__ + 2;
like(
    intercept {
        local $ENV{TS_TERM_SIZE} = 10000;
        main::warning {
            cmp_ok($foo, '&& die', $foo, 'overload exception', 'extra diag')
        }
    },
    array {
        event Exception => { error => T() };
        fail_events Ok => sub {
            call pass => 0;
            call name => 'overload exception';
        };

        event Diag => sub {
            call message => table(
                header => [qw/TYPE GOT OP CHECK/],
                rows   => [
                    ['unsupported', 'foo', '&& die', '<EXCEPTION>'],
                    ['orig', render_ref($foo), '', render_ref($foo)],
                ],
            );
        };
        event Diag => { message => 'extra diag' };

        end;
    },
    "Got exception in test"
);


note "cmp_ok() displaying good numbers"; {
    my $have = 1.23456;
    my $want = 4.5678;
    like(
        intercept {
            cmp_ok($have, '>', $want);
        },
        array {
            fail_events Ok => sub {
                call pass => 0;
            };

            event Diag => sub {
                call message => table(
                    header => [qw/GOT OP CHECK/],
                    rows   => [
                      [$have, '>', $want],
                    ],
                );
            };

            end;
        },
    );
}


note "cmp_ok() displaying bad numbers"; {
    my $have = "zero";
    my $want = "3point5";
    like(
        intercept {
            warnings { cmp_ok($have, '>', $want) };
        },
        array {
            fail_events Ok => sub {
                call pass => 0;
            };

            event Diag => sub {
                call message => table(
                    header => [qw/TYPE GOT OP CHECK/],
                    rows   => [
                      ['num',   0,      '>',    '3'],
                      ['orig',  $have,  '',     $want],
                    ],
                );
            };

            end;
        },
    );
}


done_testing;
