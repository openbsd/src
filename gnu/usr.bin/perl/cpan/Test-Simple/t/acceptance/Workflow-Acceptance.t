use strict;
use warnings;
use Test2::Require::AuthorTesting;
use Test2::Bundle::Extended;
use Test2::Tools::Spec qw/:DEFAULT include_workflow/;
use Test2::Workflow::Runner;

use Test2::API qw/intercept/;
use Test2::Util qw/get_tid/;

my $B = describe foo => sub {
    before_all start => sub { ok(1, 'start') };

    around_all al => sub {
        my $cont = shift;
        ok(1, 'al start');
        $cont->();
        ok(1, 'al end');
    };

    after_all end => sub { ok(1, 'end')   };

    before_each bef => sub { ok(1, 'a') };

    around_each arr => sub {
        my $cont = shift;
        ok(1, 'ar start');
        $cont->();
        ok(1, 'ar end');
    };

    after_each  aft => sub { ok(1, 'z') };

    case c1 => sub { ok(1, 'in c1') };
    case c2 => sub { ok(1, 'in c2') };

    before_case bc => sub { ok(1, 'in bc') };
    around_case arc => sub {
        my $cont = shift;
        ok(1, 'arc start');
        $cont->();
        ok(1, 'arc end');
    };
    after_case  ac => sub { ok(1, 'in ac') };

    tests bar => {iso => 1}, sub {
        ok(1, "inside bar pid $$ - tid " . get_tid());
    };

    tests baz => sub {
        ok(1, "inside baz pid $$ - tid " . get_tid());
    };

    tests uhg => sub {
        my $todo = todo "foo todo";
        ok(0, 'xxx');
    };

    tests bug => {todo => 'a bug'}, sub {
        ok(0, 'fail');
    };

    tests broken => {skip => 'will break things'}, sub {
        warn "\n\n**** You should not see this! ****\n\n";
        print STDERR Carp::longmess('here');
        print "not ok - You should not see this\n";
        exit 255;
    };

    describe nested => {iso => 1}, sub {
        before_each n1_be => sub { ok(1, 'nested before') };
        after_each  n1_ae => sub { ok(1, 'nested after') };

        tests n1 => sub { ok(1, 'nested 1') };
        tests n2 => sub { ok(1, 'nested 2') };
    };
};

my $r1 = Test2::Workflow::Runner->new(task => $B, no_threads => 1);
$r1->run;

my $r2 = Test2::Workflow::Runner->new(task => $B, no_fork => 1);
$r2->run;

my $r3 = Test2::Workflow::Runner->new(task => $B, no_fork => 1, no_threads => 1);
$r3->run;

tests on_root => sub { ok(1, "in root") };

{
    package Foo::Bar;

    sub foo { 'xxx' }
}

describe in_root => {flat => 1}, sub {
    is(Foo::Bar->foo, 'xxx', "not mocked");

    mock 'Foo::Bar' => (
        override => [
            foo => sub { 'foo' },
        ],
    );

    is(Foo::Bar->foo, 'foo', "mocked");

    tests on_root_a => sub {
        ok(1, "in root");
        is(Foo::Bar->foo, 'foo', "mocked");
    };

    describe 'iso-in-iso' => {iso => 1}, sub {
        tests on_root_b => {iso => 1}, sub { ok(1, "in root") };
        tests on_root_c => {iso => 1}, sub { ok(1, "in root") };
        tests on_root_d => {iso => 1}, sub { ok(1, "in root") };
    };

    my $B = describe included => sub {
        tests inside => sub { ok(1, "xxx") };
    };
    include_workflow($B);
};

is(Foo::Bar->foo, 'xxx', "not mocked");

describe todo_desc => {todo => 'cause'}, sub {
    ok(0, "not ready");

    tests foo => sub {
        ok(0, "not ready nested");
    }
};

describe skip_desc => {skip => 'cause'}, sub {
    print STDERR "Should not see this!\n";
    print "not ok - You should not see this\n";
    exit 255;
};

eval {
    describe dies => sub {
        ok(1, 'xxx');
        ok(1, 'xxx');
        die "xxx";
    };
    1;
};
like(
    $@,
    check_set(
        qr/^Exception in build 'dies' with 2 unseen event\(s\)\.$/m,
        qr{^xxx at .*Acceptance\.t line \d+\.$}m,
        qr/^Overview of unseen events:/m,
        qr/^    Test2::Event::Ok at .*Acceptance\.t line \d+$/m,
        qr/^    Test2::Event::Ok at .*Acceptance\.t line \d+/m,
    ),
    "Error is as expected"
);

my $events = intercept {
    my $r = Test2::Workflow::Runner->new(task => $B, no_fork => 1, no_threads => 1, rand => 0);
    $r->run;
};

is(
    $events,
    array {
        event Subtest => sub {
            call name           => 'foo';
            call pass           => 1;
            call effective_pass => 1;

            prop file => match qr{\QAcceptance.t\E$};
            prop line => 77;

            call subevents => array {
                event Ok => sub {
                    call name           => 'start';
                    call pass           => 1;
                    call effective_pass => 1;

                    prop file => match qr{\QAcceptance.t\E$};
                    prop line => 12;
                };

                event Ok => sub {
                    call name           => 'al start';
                    call pass           => 1;
                    call effective_pass => 1;

                    prop file => match qr{\QAcceptance.t\E$};
                    prop line => 16;
                };

                event Subtest => sub {
                    call name           => 'c1';
                    call pass           => 1;
                    call effective_pass => 1;

                    prop file => match qr{\QAcceptance.t\E$};
                    prop line => 34;

                    call subevents => array {
                        event Ok => sub {
                            call name           => 'in bc';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 37;
                        };

                        event Ok => sub {
                            call name           => 'arc start';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 40;
                        };

                        event Ok => sub {
                            call name           => 'in c1';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 34;
                        };

                        event Skip => sub {
                            call name           => 'bar';
                            call pass           => 1;
                            call effective_pass => 1;
                            call reason         => 'No isolation method available';

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 48;
                        };

                        event Subtest => sub {
                            call name           => 'baz';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 52;

                            call subevents => array {
                                event Ok => sub {
                                    call name           => 'a';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 23;
                                };

                                event Ok => sub {
                                    call name           => 'ar start';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 27;
                                };

                                event Ok => sub {
                                    call name           => match qr/inside baz pid/;
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 51;
                                };

                                event Ok => sub {
                                    call name           => 'ar end';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 29;
                                };

                                event Ok => sub {
                                    call name           => 'z';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 32;
                                };

                                event Plan => sub {
                                    call max => 5;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 52;
                                };
                                end();
                            };
                        };

                        event Subtest => sub {
                            call name           => 'uhg';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 57;

                            call subevents => array {
                                event Ok => sub {
                                    call name           => 'a';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 23;
                                };

                                event Ok => sub {
                                    call name           => 'ar start';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 27;
                                };

                                event Ok => sub {
                                    call name           => 'xxx';
                                    call pass           => 0;
                                    call effective_pass => 1;
                                    call todo           => 'foo todo';

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 56;
                                };

                                event Note => sub {
                                    call message => match qr{^\n?Failed test};

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 56;
                                };

                                event Ok => sub {
                                    call name           => 'ar end';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 29;
                                };

                                event Ok => sub {
                                    call name           => 'z';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 32;
                                };

                                event Plan => sub {
                                    call max => 5;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 57;
                                };
                                end();
                            };
                        };

                        event Subtest => sub {
                            call name           => 'bug';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 61;

                            call subevents => array {
                                event Ok => sub {
                                    call name           => 'a';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 23;
                                };

                                event Ok => sub {
                                    call name           => 'ar start';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 27;
                                };

                                event Ok => sub {
                                    call name           => 'fail';
                                    call pass           => 0;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 60;
                                };

                                event Note => sub {
                                    call message => match qr{^\n?Failed test};

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 60;
                                };

                                event Ok => sub {
                                    call name           => 'ar end';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 29;
                                };

                                event Ok => sub {
                                    call name           => 'z';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 32;
                                };

                                event Plan => sub {
                                    call max => 5;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 61;
                                };
                                end();
                            };
                        };

                        event Skip => sub {
                            call name           => 'broken';
                            call pass           => 1;
                            call effective_pass => 1;
                            call reason         => 'will break things';

                            prop file => match qr{\QRunner.pm\E$};
                        };

                        event Skip => sub {
                            call name           => 'nested';
                            call pass           => 1;
                            call effective_pass => 1;
                            call reason         => 'No isolation method available';

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 76;
                        };

                        event Ok => sub {
                            call name           => 'arc end';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 42;
                        };

                        event Ok => sub {
                            call name           => 'in ac';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 44;
                        };

                        event Plan => sub {
                            call max => 11;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 34;
                        };
                        end();
                    };
                };

                event Subtest => sub {
                    call name           => 'c2';
                    call pass           => 1;
                    call effective_pass => 1;

                    prop file => match qr{\QAcceptance.t\E$};
                    prop line => 35;

                    call subevents => array {
                        event Ok => sub {
                            call name           => 'in bc';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 37;
                        };

                        event Ok => sub {
                            call name           => 'arc start';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 40;
                        };

                        event Ok => sub {
                            call name           => 'in c2';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 35;
                        };

                        event Skip => sub {
                            call name           => 'bar';
                            call pass           => 1;
                            call effective_pass => 1;
                            call reason         => 'No isolation method available';

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 48;
                        };

                        event Subtest => sub {
                            call name           => 'baz';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 52;

                            call subevents => array {
                                event Ok => sub {
                                    call name           => 'a';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 23;
                                };

                                event Ok => sub {
                                    call name           => 'ar start';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 27;
                                };

                                event Ok => sub {
                                    call name           => match qr/inside baz pid/;
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 51;
                                };

                                event Ok => sub {
                                    call name           => 'ar end';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 29;
                                };

                                event Ok => sub {
                                    call name           => 'z';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 32;
                                };

                                event Plan => sub {
                                    call max => 5;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 52;
                                };
                                end();
                            };
                        };

                        event Subtest => sub {
                            call name           => 'uhg';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 57;

                            call subevents => array {
                                event Ok => sub {
                                    call name           => 'a';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 23;
                                };

                                event Ok => sub {
                                    call name           => 'ar start';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 27;
                                };

                                event Ok => sub {
                                    call name           => 'xxx';
                                    call pass           => 0;
                                    call effective_pass => 1;
                                    call todo           => 'foo todo';

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 56;
                                };

                                event Note => sub {
                                    call message => match qr{^\n?Failed test};

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 56;
                                };

                                event Ok => sub {
                                    call name           => 'ar end';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 29;
                                };

                                event Ok => sub {
                                    call name           => 'z';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 32;
                                };

                                event Plan => sub {
                                    call max => 5;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 57;
                                };
                                end();
                            };
                        };

                        event Subtest => sub {
                            call name           => 'bug';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 61;

                            call subevents => array {
                                event Ok => sub {
                                    call name           => 'a';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 23;
                                };

                                event Ok => sub {
                                    call name           => 'ar start';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 27;
                                };

                                event Ok => sub {
                                    call name           => 'fail';
                                    call pass           => 0;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 60;
                                };

                                event Note => sub {
                                    call message => match qr{^\n?Failed test};

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 60;
                                };

                                event Ok => sub {
                                    call name           => 'ar end';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 29;
                                };

                                event Ok => sub {
                                    call name           => 'z';
                                    call pass           => 1;
                                    call effective_pass => 1;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 32;
                                };

                                event Plan => sub {
                                    call max => 5;

                                    prop file => match qr{\QAcceptance.t\E$};
                                    prop line => 61;
                                };
                                end();
                            };
                        };

                        event Skip => sub {
                            call name           => 'broken';
                            call pass           => 1;
                            call effective_pass => 1;
                            call reason         => 'will break things';

                            prop file => match qr{\QRunner.pm\E$};
                        };

                        event Skip => sub {
                            call name           => 'nested';
                            call pass           => 1;
                            call effective_pass => 1;
                            call reason         => 'No isolation method available';

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 76;
                        };

                        event Ok => sub {
                            call name           => 'arc end';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 42;
                        };

                        event Ok => sub {
                            call name           => 'in ac';
                            call pass           => 1;
                            call effective_pass => 1;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 44;
                        };

                        event Plan => sub {
                            call max => 11;

                            prop file => match qr{\QAcceptance.t\E$};
                            prop line => 35;
                        };
                        end();
                    };
                };

                event Ok => sub {
                    call name           => 'al end';
                    call pass           => 1;
                    call effective_pass => 1;

                    prop file => match qr{\QAcceptance.t\E$};
                    prop line => 18;
                };

                event Ok => sub {
                    call name           => 'end';
                    call pass           => 1;
                    call effective_pass => 1;

                    prop file => match qr{\QAcceptance.t\E$};
                    prop line => 21;
                };

                event Plan => sub {
                    call max => 6;

                    prop file => match qr{\QAcceptance.t\E$};
                    prop line => 77;
                };
                end();
            };
        };
        end();
    },
    "Events look correct"
);

done_testing;

1;
