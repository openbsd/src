use Test2::Bundle::Extended -target => 'Test2::Tools::Basic';

{
    package Temp;
    use Test2::Tools::Basic;

    main::imported_ok(qw{
        ok pass fail diag note todo skip
        plan skip_all done_testing bail_out
    });
}

pass('Testing Pass');

my @lines;
like(
    intercept {
        pass('pass');               push @lines => __LINE__;
        fail('fail');               push @lines => __LINE__;
        fail('fail', 'added diag'); push @lines => __LINE__;
    },
    array {
        event Ok => sub {
            call pass => 1;
            call name => 'pass';

            prop file    => __FILE__;
            prop package => __PACKAGE__;
            prop line    => $lines[0];
            prop subname => 'Test2::Tools::Basic::pass';
        };

        event Ok => sub {
            call pass => 0;
            call name => 'fail';

            prop file    => __FILE__;
            prop package => __PACKAGE__;
            prop line    => $lines[1];
            prop subname => 'Test2::Tools::Basic::fail';
        };
        event Diag => sub {
            call message => qr/Failed test 'fail'.*line $lines[1]/s;

            prop file    => __FILE__;
            prop package => __PACKAGE__;
            prop line    => $lines[1];
            prop subname => 'Test2::Tools::Basic::fail';
        };

        event Ok => sub {
            call pass => 0;
            call name => 'fail';

            prop file    => __FILE__;
            prop package => __PACKAGE__;
            prop line    => $lines[2];
            prop subname => 'Test2::Tools::Basic::fail';
        };
        event Diag => sub {
            call message => qr/Failed test 'fail'.*line $lines[2]/s;

            prop file    => __FILE__;
            prop package => __PACKAGE__;
            prop line    => $lines[2];
            prop subname => 'Test2::Tools::Basic::fail';
        };
        event Diag => sub {
            call message => 'added diag';

            prop file    => __FILE__;
            prop package => __PACKAGE__;
            prop line    => $lines[2];
            prop subname => 'Test2::Tools::Basic::fail';
        };

        end;
    },
    "Got expected events for 'pass' and 'fail'"
);

ok(1, 'Testing ok');

@lines = ();
like(
    intercept {
        ok(1, 'pass', 'invisible diag'); push @lines => __LINE__;
        ok(0, 'fail');                   push @lines => __LINE__;
        ok(0, 'fail', 'added diag');     push @lines => __LINE__;
    },
    array {
        event Ok => sub {
            call pass => 1;
            call name => 'pass';
            prop line => $lines[0];
        };

        event Ok => sub {
            call pass => 0;
            call name => 'fail';
            prop debug => 'at ' . __FILE__ . " line $lines[1]";
        };
        event Diag => sub {
            call message => qr/Failed test 'fail'.*line $lines[1]/s;
            prop debug => 'at ' . __FILE__ . " line $lines[1]";
        };

        event Ok => sub {
            call pass => 0;
            call name => 'fail';
            prop debug => 'at ' . __FILE__ . " line $lines[2]";
        };
        event Diag => sub {
            call message => qr/Failed test 'fail'.*line $lines[2]/s;
            prop debug => 'at ' . __FILE__ . " line $lines[2]";
        };
        event Diag => sub {
            call message => 'added diag';
            prop debug => 'at ' . __FILE__ . " line $lines[2]";
        };

        end;
    },
    "Got expected events for 'ok'"
);

diag "Testing Diag (AUTHOR_TESTING ONLY)" if $ENV{AUTHOR_TESTING};

like(
    intercept {
        diag "foo";
        diag "foo", ' ', "bar";
    },
    array {
        event Diag => { message => 'foo' };
        event Diag => { message => 'foo bar' };
    },
    "Got expected events for diag"
);

note "Testing Note";

like(
    intercept {
        note "foo";
        note "foo", ' ', "bar";
    },
    array {
        event Note => { message => 'foo' };
        event Note => { message => 'foo bar' };
    },
    "Got expected events for note"
);

like(
    intercept {
        bail_out 'oops';
        # Should not get here
        print STDERR "Something is wrong, did not bail out!\n";
        exit 255;
    },
    array {
        event Bail => { reason => 'oops' };
        end;
    },
    "Got bail event"
);

like(
    intercept {
        skip_all 'oops';
        # Should not get here
        print STDERR "Something is wrong, did not skip!\n";
        exit 255;
    },
    array {
        event Plan => { max => 0, directive => 'SKIP', reason => 'oops' };
        end;
    },
    "Got plan (skip_all) event"
);

like(
    intercept {
        plan skip_all => 'oops';
        # Should not get here
        print STDERR "Something is wrong, did not skip!\n";
        exit 255;
    },
    array {
        event Plan => { max => 0, directive => 'SKIP', reason => 'oops' };
        end;
    },
    "Got plan 'skip_all' prefix"
);


like(
    intercept {
        plan(5);
    },
    array {
        event Plan => { max => 5 };
        end;
    },
    "Got plan"
);

like(
    intercept {
        plan(tests => 5);
    },
    array {
        event Plan => { max => 5 };
        end;
    },
    "Got plan 'tests' prefix"
);


like(
    intercept {
        ok(1);
        ok(2);
        done_testing;
    },
    array {
        event Ok => { pass => 1 };
        event Ok => { pass => 1 };
        event Plan => { max => 2 };
        end;
    },
    "Done Testing works"
);

like(
    intercept {
        ok(0, "not todo");

        {
            my $todo = todo('todo 1');
            ok(0, 'todo fail');
        }

        ok(0, "not todo");

        my $todo = todo('todo 2');
        ok(0, 'todo fail');
        $todo = undef;

        ok(0, "not todo");

        todo 'todo 3' => sub {
            ok(0, 'todo fail');
        };

        ok(0, "not todo");
    },
    array {
        for my $id (1 .. 3) {
            event Ok => sub {
                call pass => 0;
                call effective_pass => 0;
                call todo => undef;
            };
            event Diag => { message => qr/Failed/ };

            event Ok => sub {
                call pass => 0;
                call effective_pass => 1;
                call todo => "todo $id";
            };
            event Note => { message => qr/Failed/ };
        }
        event Ok => { pass => 0, effective_pass => 0 };
        event Diag => { message => qr/Failed/ };
        end;
    },
    "Got todo events"
);

like(
    intercept {
        ok(1, 'pass');
        SKIP: {
            skip 'oops' => 5;

            ok(1, "Should not see this");
        }
    },
    array {
        event Ok => { pass => 1 };

        event Skip => sub {
            call pass => 1;
            call reason => 'oops';
        } for 1 .. 5;

        end;
    },
    "got skip events"
);

done_testing;
