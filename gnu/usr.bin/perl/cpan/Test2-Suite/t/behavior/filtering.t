use Test2::Bundle::Extended;
use Test2::Tools::Spec -rand => 0;
use Test2::Workflow::Runner;

my %LINES;

sub example {
    my $unit = describe root => {flat => 1}, sub {
        before_all  'root_before_all'  => sub { note "root_before_all"  };
        after_all   'root_after_all'   => sub { note 'root_after_all'   };
        before_each 'root_before_each' => sub { note 'root_before_each' };
        after_each  'root_after_each'  => sub { note 'root_after_each'  };

        around_all 'root_around_all' => sub {
            note 'root_around_all_prefix';
            $_[0]->();
            note 'root_around_all_postfix';
        };

        around_each 'root_around_each' => sub {
            note 'root_around_each_prefix';
            $_[0]->();
            note 'root_around_each_postfix';
        };

        case root_x => sub { note 'root case x' }; BEGIN { $LINES{root_x} = __LINE__ }
        case root_y => sub { note 'root case y' }; BEGIN { $LINES{root_y} = __LINE__ }

        tests 'root_a' => sub { ok(1, 'root_a') }; BEGIN { $LINES{root_a} = __LINE__ }
        tests 'root_b' => sub { ok(1, 'root_b') }; BEGIN { $LINES{root_b} = __LINE__ }

        tests 'root_long' => sub {
            ok(1, 'root_long_1');

            BEGIN { $LINES{root_long} = __LINE__ }
            # Intentional space

            ok(1, 'root_long_2');
        };

        tests dup_name => sub { ok(1, 'dup_name') };

        describe nested => sub {
            before_all  'nested_before_all'  => sub { note "nested_before_all"  };
            after_all   'nested_after_all'   => sub { note 'nested_after_all'   };
            before_each 'nested_before_each' => sub { note 'nested_before_each' };
            after_each  'nested_after_each'  => sub { note 'nested_after_each'  };

            around_all 'nested_around_all' => sub {
                note 'nested_around_all_prefix';
                $_[0]->();
                note 'nested_around_all_postfix';
            };

            around_each 'nested_around_each' => sub {
                note 'nested_around_each_prefix';
                $_[0]->();
                note 'nested_around_each_postfix';
            };

            BEGIN { $LINES{nested} = __LINE__ }

            case nested_x => sub { note 'nested case x' }; BEGIN { $LINES{nested_x} = __LINE__ }
            case nested_y => sub { note 'nested case y' }; BEGIN { $LINES{nested_y} = __LINE__ }

            tests 'nested_a' => sub { ok(1, 'nested_a') }; BEGIN { $LINES{nested_a} = __LINE__ }
            tests 'nested_b' => sub { ok(1, 'nested_b') }; BEGIN { $LINES{nested_b} = __LINE__ }

            tests 'nested_long' => sub {
                ok(1, 'nested_long_1');

                BEGIN { $LINES{nested_long} = __LINE__ }
                # Intentional space

                ok(1, 'nested_long_2');
            };

            tests dup_name => sub { ok(1, 'dup_name') };
        };
    };
    return $unit;
};

describe root_test => sub {
    my $filter;
    my $type;
    case line => {flat => 1}, sub { $type = 'line'; $filter = {line => $LINES{root_long}} };
    case name => {flat => 1}, sub { $type = 'name'; $filter = {name => 'root_long'} };

    tests root => {flat => 1}, sub {
        my $unit = example();

        my $events = intercept {
            Test2::Workflow::Runner->new(
                rand => 0,
                task => $unit,
                filter => $filter
            )->run();
        };

        is(
            $events,
            array {
                event Note => { message => 'root_before_all' };
                event Note => { message => 'root_around_all_prefix' };

                event Subtest => sub {
                    call name => "root_$_";
                    call subevents => array {
                        event Note => { message => "root case $_" };

                        event Skip => {};
                        event Skip => {};

                        event Subtest => sub {
                            call name => 'root_long';
                            call subevents => array {
                                event Note => { message => 'root_before_each' };
                                event Note => { message => 'root_around_each_prefix' };

                                event Ok => { name => 'root_long_1' };
                                event Ok => { name => 'root_long_2' };

                                event Note => { message => 'root_after_each' };
                                event Note => { message => 'root_around_each_postfix' };
                                event Plan => { max => 2 };
                            };
                        };

                        event Skip => {};
                        event Skip => {};

                        event Plan => { max => 5 };
                    };
                } for qw/x y/;

                event Note => { message => 'root_after_all' };
                event Note => { message => 'root_around_all_postfix' };
            },
            "Got only the events that match the $type filter"
        );
    };
};

describe nested_test => sub {
    my $filter;
    my $type;
    case line => {flat => 1}, sub { $type = 'line'; $filter = {line => $LINES{nested_long}} };
    case name => {flat => 1}, sub { $type = 'name'; $filter = {name => 'nested_long'} };

    tests nested => {flat => 1}, sub {
        my $unit = example();

        my $events = intercept {
            Test2::Workflow::Runner->new(
                rand => 0,
                task => $unit,
                filter => $filter
            )->run();
        };

        is(
            $events,
            array {
                event Note => { message => 'root_before_all' };
                event Note => { message => 'root_around_all_prefix' };

                event Subtest => sub {
                    call name => "root_$_";
                    call subevents => array {
                        event Note => { message => "root case $_" };

                        event Skip => {};
                        event Skip => {};
                        event Skip => {};
                        event Skip => {};

                        event Subtest => sub {
                            call name => 'nested';
                            call subevents => array {
                                event Note => { message => 'nested_before_all' };
                                event Note => { message => 'nested_around_all_prefix' };

                                event Subtest => sub {
                                    call name => "nested_$_";
                                    call subevents => array {
                                        event Note => { message => "nested case $_" };

                                        event Skip => {};
                                        event Skip => {};

                                        event Subtest => sub {
                                            call name => 'nested_long';
                                            call subevents => array {
                                                event Note => { message => 'root_before_each' };
                                                event Note => { message => 'root_around_each_prefix' };
                                                event Note => { message => 'nested_before_each' };
                                                event Note => { message => 'nested_around_each_prefix' };

                                                event Ok => { name => 'nested_long_1' };
                                                event Ok => { name => 'nested_long_2' };

                                                event Note => { message => 'nested_after_each' };
                                                event Note => { message => 'nested_around_each_postfix' };
                                                event Note => { message => 'root_after_each' };
                                                event Note => { message => 'root_around_each_postfix' };

                                                event Plan => { max => 2 };
                                            };
                                        };

                                        event Skip => {};

                                        event Plan => { max => 4 };
                                    };
                                } for qw/x y/;

                                event Note => { message => 'nested_after_all' };
                                event Note => { message => 'nested_around_all_postfix' };

                                event Plan => { max => 2 };
                            };
                        };

                        event Plan => { max => 5 };
                    };
                } for qw/x y/;

                event Note => { message => 'root_after_all' };
                event Note => { message => 'root_around_all_postfix' };
            },
            "Got only the events that match the $type filter"
        );
    };
};

describe group => sub {
    my $filter;
    my $type;
    case line => {flat => 1}, sub { $type = 'line'; $filter = {line => $LINES{nested}} };
    case name => {flat => 1}, sub { $type = 'name'; $filter = {name => 'nested'} };

    tests nested => {flat => 1}, sub {
        my $unit = example();

        my $events = intercept {
            Test2::Workflow::Runner->new(
                rand => 0,
                task => $unit,
                filter => $filter
            )->run();
        };

        is(
            $events,
            array {
                event Note => { message => 'root_before_all' };
                event Note => { message => 'root_around_all_prefix' };

                event Subtest => sub {
                    call name => "root_$_";
                    call subevents => array {
                        event Note => { message => "root case $_" };

                        event Skip => {};
                        event Skip => {};
                        event Skip => {};
                        event Skip => {};

                        event Subtest => sub {
                            call name => 'nested';
                            call subevents => array {
                                event Note => { message => 'nested_before_all' };
                                event Note => { message => 'nested_around_all_prefix' };

                                event Subtest => sub {
                                    call name => "nested_$_";
                                    call subevents => array {
                                        event Note => { message => "nested case $_" };

                                        event Subtest => sub {
                                            call name => $_;
                                            call subevents => array {
                                                event Note => { message => 'root_before_each' };
                                                event Note => { message => 'root_around_each_prefix' };
                                                event Note => { message => 'nested_before_each' };
                                                event Note => { message => 'nested_around_each_prefix' };

                                                if ($_ eq 'nested_long') {
                                                    event Ok => { name => 'nested_long_1' };
                                                    event Ok => { name => 'nested_long_2' };
                                                }
                                                else {
                                                    event Ok => { name => $_ };
                                                }

                                                event Note => { message => 'nested_after_each' };
                                                event Note => { message => 'nested_around_each_postfix' };
                                                event Note => { message => 'root_after_each' };
                                                event Note => { message => 'root_around_each_postfix' };

                                                event Plan => { max => T() };
                                            };
                                        } for qw/nested_a nested_b nested_long dup_name/;

                                        event Plan => { max => 4 };
                                    };
                                } for qw/x y/;

                                event Note => { message => 'nested_after_all' };
                                event Note => { message => 'nested_around_all_postfix' };

                                event Plan => { max => 2 };
                            };
                        };

                        event Plan => { max => 5 };
                    };
                } for qw/x y/;

                event Note => { message => 'root_after_all' };
                event Note => { message => 'root_around_all_postfix' };
            },
            "Got only the events that match the $type filter"
        );
    };
};

tests dup_name => sub {
    my $unit = example();

    my $events = intercept {
        Test2::Workflow::Runner->new(
            rand => 0,
            task => $unit,
            filter => {name => 'dup_name'}
        )->run();
    };

    is(
        $events,
        array {
            event Note => { message => 'root_before_all' };
            event Note => { message => 'root_around_all_prefix' };

            event Subtest => sub {
                call name => "root_$_";
                call subevents => array {
                    event Note => { message => "root case $_" };

                    event Skip => {};
                    event Skip => {};
                    event Skip => {};

                    event Subtest => sub {
                        call name => 'dup_name';
                        call subevents => array {
                            event Note => { message => 'root_before_each' };
                            event Note => { message => 'root_around_each_prefix' };

                            event Ok => { name => 'dup_name' };

                            event Note => { message => 'root_after_each' };
                            event Note => { message => 'root_around_each_postfix' };

                            event Plan => { max => 1 };
                        };
                    };

                    event Subtest => sub {
                        call name => 'nested';
                        call subevents => array {
                            event Note => { message => 'nested_before_all' };
                            event Note => { message => 'nested_around_all_prefix' };

                            event Subtest => sub {
                                call name => "nested_$_";
                                call subevents => array {
                                    event Note => { message => "nested case $_" };

                                    event Skip => {};
                                    event Skip => {};
                                    event Skip => {};

                                    event Subtest => sub {
                                        call name => 'dup_name';
                                        call subevents => array {
                                            event Note => { message => 'root_before_each' };
                                            event Note => { message => 'root_around_each_prefix' };
                                            event Note => { message => 'nested_before_each' };
                                            event Note => { message => 'nested_around_each_prefix' };

                                            event Ok => { name => 'dup_name' };

                                            event Note => { message => 'nested_after_each' };
                                            event Note => { message => 'nested_around_each_postfix' };
                                            event Note => { message => 'root_after_each' };
                                            event Note => { message => 'root_around_each_postfix' };

                                            event Plan => { max => 1 };
                                        };
                                    };

                                    event Plan => { max => 4 };
                                };
                            } for qw/x y/;

                            event Note => { message => 'nested_after_all' };
                            event Note => { message => 'nested_around_all_postfix' };

                            event Plan => { max => 2 };
                        };
                    };

                    event Plan => { max => 5 };
                };
            } for qw/x y/;

            event Note => { message => 'root_after_all' };
            event Note => { message => 'root_around_all_postfix' };
        },
        "Got only the events that match the dup_name filter"
    );
};

tests root_case => sub {
    my $unit = example();

    my $events = intercept {
        Test2::Workflow::Runner->new(
            rand => 0,
            task => $unit,
            filter => {name => 'root_x'}
        )->run();
    };

    is(
        $events,
        array {
            event Note => { message => 'root_before_all' };
            event Note => { message => 'root_around_all_prefix' };

            event Subtest => sub {
                call name => "root_x";
                call subevents => array {
                    event Note => { message => "root case x" };

                    event Subtest => sub {
                        call name => $_;
                        call subevents => array {
                            event Note => { message => 'root_before_each' };
                            event Note => { message => 'root_around_each_prefix' };

                            if ($_ eq 'root_long') {
                                event Ok => { name => 'root_long_1' };
                                event Ok => { name => 'root_long_2' };
                            }
                            else {
                                event Ok => { name => $_ };
                            }

                            event Note => { message => 'root_after_each' };
                            event Note => { message => 'root_around_each_postfix' };

                            event Plan => { max => T() };
                        };
                    } for qw/root_a root_b root_long dup_name/;

                    event Subtest => sub {
                        call name => 'nested';
                        call subevents => array {
                            event Note => { message => 'nested_before_all' };
                            event Note => { message => 'nested_around_all_prefix' };

                            event Subtest => sub {
                                call name => "nested_$_";
                                call subevents => array {
                                    event Note => { message => "nested case $_" };

                                    event Subtest => sub {
                                        call name => $_;
                                        call subevents => array {
                                            event Note => { message => 'root_before_each' };
                                            event Note => { message => 'root_around_each_prefix' };
                                            event Note => { message => 'nested_before_each' };
                                            event Note => { message => 'nested_around_each_prefix' };

                                            if ($_ eq 'nested_long') {
                                                event Ok => { name => 'nested_long_1' };
                                                event Ok => { name => 'nested_long_2' };
                                            }
                                            else {
                                                event Ok => { name => $_ };
                                            }

                                            event Note => { message => 'nested_after_each' };
                                            event Note => { message => 'nested_around_each_postfix' };
                                            event Note => { message => 'root_after_each' };
                                            event Note => { message => 'root_around_each_postfix' };

                                            event Plan => { max => T() };
                                        };
                                    } for qw/nested_a nested_b nested_long dup_name/;

                                    event Plan => { max => 4 };
                                };
                            } for qw/x y/;

                            event Note => { message => 'nested_after_all' };
                            event Note => { message => 'nested_around_all_postfix' };

                            event Plan => { max => 2 };
                        };
                    };

                    event Plan => { max => 5 };
                };
            };

            event Skip => {};

            event Note => { message => 'root_after_all' };
            event Note => { message => 'root_around_all_postfix' };
        },
        "Got only the events that match the case filter"
    );
};

tests nested_case => sub {
    my $unit = example();

    my $events = intercept {
        Test2::Workflow::Runner->new(
            rand => 0,
            task => $unit,
            filter => {name => 'nested_x'}
        )->run();
    };

    is(
        $events,
        array {
            event Note => { message => 'root_before_all' };
            event Note => { message => 'root_around_all_prefix' };

            event Subtest => sub {
                call name => "root_$_";
                call subevents => array {
                    event Note => { message => "root case $_" };

                    event Skip => {};
                    event Skip => {};
                    event Skip => {};
                    event Skip => {};

                    event Subtest => sub {
                        call name => 'nested';
                        call subevents => array {
                            event Note => { message => 'nested_before_all' };
                            event Note => { message => 'nested_around_all_prefix' };

                            event Subtest => sub {
                                call name => "nested_x";
                                call subevents => array {
                                    event Note => { message => "nested case x" };

                                    event Subtest => sub {
                                        call name => $_;
                                        call subevents => array {
                                            event Note => { message => 'root_before_each' };
                                            event Note => { message => 'root_around_each_prefix' };
                                            event Note => { message => 'nested_before_each' };
                                            event Note => { message => 'nested_around_each_prefix' };

                                            if ($_ eq 'nested_long') {
                                                event Ok => { name => 'nested_long_1' };
                                                event Ok => { name => 'nested_long_2' };
                                            }
                                            else {
                                                event Ok => { name => $_ };
                                            }

                                            event Note => { message => 'nested_after_each' };
                                            event Note => { message => 'nested_around_each_postfix' };
                                            event Note => { message => 'root_after_each' };
                                            event Note => { message => 'root_around_each_postfix' };

                                            event Plan => { max => T() };
                                        };
                                    } for qw/nested_a nested_b nested_long dup_name/;

                                    event Plan => { max => 4 };
                                };
                            };

                            event Skip => {};

                            event Note => { message => 'nested_after_all' };
                            event Note => { message => 'nested_around_all_postfix' };

                            event Plan => { max => 2 };
                        };
                    };

                    event Plan => { max => 5 };
                };
            } for qw/x y/;

            event Note => { message => 'root_after_all' };
            event Note => { message => 'root_around_all_postfix' };
        },
        "Got only the events that match the nested case filter"
    );
};

done_testing;
