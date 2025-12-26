use Test2::Bundle::Extended -target => 'Test2::Tools::Subtest';

use Test2::Tools::Subtest qw/subtest_streamed subtest_buffered/;


use File::Temp qw/tempfile/;

# A bug in older perls causes a strange error AFTER the program appears to be
# done if this test is run.
# "Size magic not implemented."
if ($] > 5.020000 && $ENV{AUTHOR_TESTING}) {
    like(
        intercept {
            subtest_streamed 'foo' => sub {
                my ($fh, $name) = tempfile;
                print $fh <<"                EOT";
                    use Test2::Bundle::Extended;
                    BEGIN { skip_all 'because' }
                    1;
                EOT
                close($fh);
                do $name;
                unlink($name) or warn "Could not remove temp file $name: $!";
                die $@ if $@;
                die "Ooops";
            };
        },
        subset {
            event Note => { message => 'Subtest: foo' };
            event Subtest => sub {
                field pass => 1;
                field name => 'Subtest: foo';
                field subevents => subset {
                    event Plan => { directive => 'SKIP', reason => 'because' };
                };
            }
        },
        "skip_all in BEGIN inside a subtest works"
    );
}

subtest_streamed 'hub tests' => sub {
    my $hub = Test2::API::test2_stack->top;
    isa_ok($hub, 'Test2::Hub', 'Test2::Hub::Subtest');

    my $todo = todo "testing parent_todo";
    subtest_streamed 'inner hub tests' => sub {
        my $ihub = Test2::API::test2_stack->top;
        isa_ok($ihub, 'Test2::Hub', 'Test2::Hub::Subtest');
    };
};

like(
    intercept {
        subtest_streamed 'foo' => sub {
            subtest_buffered 'bar' => sub {
                ok(1, "pass");
            };
        };
    },
    subset {
        event Note => { message => 'Subtest: foo' };
        event Subtest => sub {
            field pass => 1;
            field name => 'Subtest: foo';
            field subevents => subset {
                event Subtest => sub {
                    field pass => 1;
                    field name => 'bar';
                    field subevents => subset {
                        event Ok => sub {
                            field name => 'pass';
                            field pass => 1;
                        };
                    };
                };
            };
        };
    },
    "Can nest subtests"
);

my @lines = ();
like(
    intercept {
        push @lines => __LINE__ + 4;
        subtest_streamed 'foo' => sub {
            push @lines => __LINE__ + 1;
            ok(1, "pass");
        };
    },
    subset {
        event Note => { message => 'Subtest: foo' };
        event Subtest => sub {
            prop file => __FILE__;
            prop line => $lines[0];
            field pass => 1;
            field name => 'Subtest: foo';
            field subevents => subset {
                event Ok => sub {
                    prop file => __FILE__;
                    prop line => $lines[1];
                    field name => 'pass';
                    field pass => 1;
                };
            };
        };
    },
    "Got events for passing subtest"
);

@lines = ();
like(
    intercept {
        push @lines => __LINE__ + 4;
        subtest_streamed 'foo' => sub {
            push @lines => __LINE__ + 1;
            ok(0, "fail");
        };
    },
    subset {
        event Note => { message => 'Subtest: foo' };
        event Subtest => sub {
            prop file => __FILE__;
            prop line => $lines[0];
            field pass => 0;
            field name => 'Subtest: foo';
            field subevents => subset {
                event Ok => sub {
                    prop file => __FILE__;
                    prop line => $lines[1];
                    field name => 'fail';
                    field pass => 0;
                };
            };
        };
    },
    "Got events for failing subtest"
);

@lines = ();
like(
    intercept {
        push @lines => __LINE__ + 5;
        subtest_streamed 'foo' => sub {
            push @lines => __LINE__ + 1;
            ok(1, "pass");
            done_testing;
        };
    },
    subset {
        event Note => { message => 'Subtest: foo' };
        event Subtest => sub {
            prop file => __FILE__;
            prop line => $lines[0];
            field pass => 1;
            field name => 'Subtest: foo';
            field subevents => subset {
                event Ok => sub {
                    prop file => __FILE__;
                    prop line => $lines[1];
                    field name => 'pass';
                    field pass => 1;
                };
                event Plan => { max => 1 };
            };
        };
    },
    "Can use done_testing"
);

@lines = ();
like(
    intercept {
        push @lines => __LINE__ + 5;
        subtest_streamed 'foo' => sub {
            plan 1;
            push @lines => __LINE__ + 1;
            ok(1, "pass");
        };
    },
    subset {
        event Note => { message => 'Subtest: foo' };
        event Subtest => sub {
            prop file => __FILE__;
            prop line => $lines[0];
            field pass => 1;
            field name => 'Subtest: foo';
            field subevents => subset {
                event Plan => { max => 1 };
                event Ok => sub {
                    prop file => __FILE__;
                    prop line => $lines[1];
                    field name => 'pass';
                    field pass => 1;
                };
            };
        };
    },
    "Can plan"
);

@lines = ();
like(
    intercept {
        push @lines => __LINE__ + 5;
        subtest_streamed 'foo' => sub {
            skip_all 'bleh';
            push @lines => __LINE__ + 1;
            ok(1, "pass");
        };
    },
    subset {
        event Note => { message => 'Subtest: foo' };
        event Subtest => sub {
            prop file => __FILE__;
            prop line => $lines[0];
            field pass => 1;
            field name => 'Subtest: foo';
            field subevents => subset {
                event Plan => { directive => 'SKIP', reason => 'bleh' };
            };
        };
    },
    "Can skip_all"
);

@lines = ();
like(
    intercept {
        subtest_streamed 'foo' => sub {
            bail_out 'cause';
            ok(1, "should not see this");
        };
    },
    subset {
        event Note => { message => 'Subtest: foo' };
        event Bail => { reason => 'cause' };
    },
    "Can bail out"
);

@lines = ();
like(
    intercept {
        push @lines => __LINE__ + 4;
        subtest_buffered 'foo' => sub {
            push @lines => __LINE__ + 1;
            ok(1, "pass");
        };
    },
    subset {
        event Subtest => sub {
            prop file => __FILE__;
            prop line => $lines[0];
            field pass => 1;
            field name => 'foo';
            field subevents => subset {
                event Ok => sub {
                    prop file => __FILE__;
                    prop line => $lines[1];
                    field name => 'pass';
                    field pass => 1;
                };
            };
        };
    },
    "Got events for passing subtest"
);

@lines = ();
like(
    intercept {
        push @lines => __LINE__ + 4;
        subtest_buffered 'foo' => sub {
            push @lines => __LINE__ + 1;
            ok(0, "fail");
        };
    },
    subset {
        event Subtest => sub {
            prop file => __FILE__;
            prop line => $lines[0];
            field pass => 0;
            field name => 'foo';
            field subevents => subset {
                event Ok => sub {
                    prop file => __FILE__;
                    prop line => $lines[1];
                    field name => 'fail';
                    field pass => 0;
                };
            };
        };
    },
    "Got events for failing subtest"
);

@lines = ();
like(
    intercept {
        push @lines => __LINE__ + 5;
        subtest_buffered 'foo' => sub {
            push @lines => __LINE__ + 1;
            ok(1, "pass");
            done_testing;
        };
    },
    subset {
        event Subtest => sub {
            prop file => __FILE__;
            prop line => $lines[0];
            field pass => 1;
            field name => 'foo';
            field subevents => subset {
                event Ok => sub {
                    prop file => __FILE__;
                    prop line => $lines[1];
                    field name => 'pass';
                    field pass => 1;
                };
                event Plan => { max => 1 };
            };
        };
    },
    "Can use done_testing"
);

@lines = ();
like(
    intercept {
        push @lines => __LINE__ + 5;
        subtest_buffered 'foo' => sub {
            plan 1;
            push @lines => __LINE__ + 1;
            ok(1, "pass");
        };
    },
    subset {
        event Subtest => sub {
            prop file => __FILE__;
            prop line => $lines[0];
            field pass => 1;
            field name => 'foo';
            field subevents => subset {
                event Plan => { max => 1 };
                event Ok => sub {
                    prop file => __FILE__;
                    prop line => $lines[1];
                    field name => 'pass';
                    field pass => 1;
                };
            };
        };
    },
    "Can plan"
);

@lines = ();
like(
    intercept {
        push @lines => __LINE__ + 5;
        subtest_buffered 'foo' => sub {
            skip_all 'bleh';
            push @lines => __LINE__ + 1;
            ok(1, "pass");
        };
    },
    subset {
        event Subtest => sub {
            prop file => __FILE__;
            prop line => $lines[0];
            field pass => 1;
            field name => 'foo';
            field subevents => subset {
                event Plan => { directive => 'SKIP', reason => 'bleh' };
            };
        };
    },
    "Can skip_all"
);

@lines = ();
like(
    intercept {
        subtest_buffered 'foo' => sub {
            bail_out 'cause';
            ok(1, "should not see this");
        };
    },
    subset {
        event Bail => { reason => 'cause' };
    },
    "Can bail out"
);

@lines = ();
my $xyz = 0;
like(
    intercept {
        push @lines => __LINE__ + 5;
        subtest_buffered 'foo' => {manual_skip_all => 1}, sub {
            skip_all 'bleh';
            $xyz = 1;
            return;
        };
    },
    subset {
        event Subtest => sub {
            prop file => __FILE__;
            prop line => $lines[0];
            field pass => 1;
            field name => 'foo';
            field subevents => subset {
                event Plan => { directive => 'SKIP', reason => 'bleh' };
            };
        };
    },
    "Can skip_all"
);
ok($xyz, "skip_all did not auto-abort");

done_testing;
