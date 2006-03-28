#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use Test::More;
use File::Spec;

my $Curdir = File::Spec->curdir;
my $SAMPLE_TESTS = $ENV{PERL_CORE}
                    ? File::Spec->catdir($Curdir, 'lib', 'sample-tests')
                    : File::Spec->catdir($Curdir, 't',   'sample-tests');


my $IsMacPerl = $^O eq 'MacOS';
my $IsVMS     = $^O eq 'VMS';

# VMS uses native, not POSIX, exit codes.
my $die_exit = $IsVMS ? 44 : 1;

# We can only predict that the wait status should be zero or not.
my $wait_non_zero = 1;

my %samples = (
    bignum => {
        bonus => 0,
        details => [
            {
                actual_ok => 1,
                ok => 1
            },
            {
                actual_ok => 1,
                ok => 1
            }
        ],
        'exit' => 0,
        max => 2,
        ok => 4,
        passing => 0,
        seen => 4,
        skip => 0,
        todo => 0,
        'wait' => 0
    },
    combined => {
        bonus => 1,
        details => [
            {
                actual_ok => 1,
                ok => 1
            },
            {
                actual_ok => 1,
                name => "basset hounds got long ears",
                ok => 1
            },
            {
                actual_ok => 0,
                name => "all hell broke lose",
                ok => 0
            },
            {
                actual_ok => 1,
                ok => 1,
                type => "todo"
            },
            {
                actual_ok => 1,
                ok => 1
            },
            {
                actual_ok => 1,
                ok => 1
            },
            {
                actual_ok => 1,
                ok => 1,
                reason => "contract negociations",
                type => "skip"
            },
            {
                actual_ok => 1,
                ok => 1
            },
            {
                actual_ok => 0,
                ok => 0
            },
            {
                actual_ok => 0,
                ok => 1,
                type => "todo"
            }
        ],
        'exit' => 0,
        max => 10,
        ok => 8,
        passing => 0,
        seen => 10,
        skip => 1,
        todo => 2,
        'wait' => 0
    },
    descriptive => {
        bonus => 0,
        details => [
            {
                actual_ok => 1,
                name => "Interlock activated",
                ok => 1
            },
            {
                actual_ok => 1,
                name => "Megathrusters are go",
                ok => 1
            },
            {
                actual_ok => 1,
                name => "Head formed",
                ok => 1
            },
            {
                actual_ok => 1,
                name => "Blazing sword formed",
                ok => 1
            },
            {
                actual_ok => 1,
                name => "Robeast destroyed",
                ok => 1
            }
        ],
        'exit' => 0,
        max => 5,
        ok => 5,
        passing => 1,
        seen => 5,
        skip => 0,
        todo => 0,
        'wait' => 0
    },
    'die' => {
        bonus => 0,
        details => [],
        'exit' => $die_exit,
        max => 0,
        ok => 0,
        passing => 0,
        seen => 0,
        skip => 0,
        todo => 0,
        'wait' => $wait_non_zero
    },
    die_head_end => {
        bonus => 0,
        details => [
            ({
                actual_ok => 1,
                ok => 1
            }) x 4,
        ],
        'exit' => $die_exit,
        max => 0,
        ok => 4,
        passing => 0,
        seen => 4,
        skip => 0,
        todo => 0,
        'wait' => $wait_non_zero
    },
    die_last_minute => {
        bonus => 0,
        details => [
            ({
                actual_ok => 1,
                ok => 1
            }) x 4,
        ],
        'exit' => $die_exit,
        max => 4,
        ok => 4,
        passing => 0,
        seen => 4,
        skip => 0,
        todo => 0,
        'wait' => $wait_non_zero
    },
    duplicates => {
        bonus => 0,
        details => [
            ({
                actual_ok => 1,
                ok => 1
            }) x 10,
        ],
        'exit' => 0,
        max => 10,
        ok => 11,
        passing => 0,
        seen => 11,
        skip => 0,
        todo => 0,
        'wait' => 0
    },
    head_end => {
        bonus => 0,
        details => [
            ({
                actual_ok => 1,
                ok => 1
            }) x 3,
            {
                actual_ok => 1,
                diagnostics => "comment\nmore ignored stuff\nand yet more\n",
                ok => 1
            }
        ],
        'exit' => 0,
        max => 4,
        ok => 4,
        passing => 1,
        seen => 4,
        skip => 0,
        todo => 0,
        'wait' => 0
    },
    head_fail => {
        bonus => 0,
        details => [
            {
                actual_ok => 1,
                ok => 1
            },
            {
                actual_ok => 0,
                ok => 0
            },
            {
                actual_ok => 1,
                ok => 1
            },
            {
                actual_ok => 1,
                diagnostics => "comment\nmore ignored stuff\nand yet more\n",
                ok => 1
            }
        ],
        'exit' => 0,
        max => 4,
        ok => 3,
        passing => 0,
        seen => 4,
        skip => 0,
        todo => 0,
        'wait' => 0
    },
    lone_not_bug => {
        bonus => 0,
        details => [
            ({
                actual_ok => 1,
                ok => 1
            }) x 4,
        ],
        'exit' => 0,
        max => 4,
        ok => 4,
        passing => 1,
        seen => 4,
        skip => 0,
        todo => 0,
        'wait' => 0
    },
    no_output => {
        bonus => 0,
        details => [],
        'exit' => 0,
        max => 0,
        ok => 0,
        passing => 0,
        seen => 0,
        skip => 0,
        todo => 0,
        'wait' => 0
    },
    shbang_misparse => {
        bonus => 0,
        details => [
            ({
                actual_ok => 1,
                ok => 1
            }) x 2,
        ],
        'exit' => 0,
        max => 2,
        ok => 2,
        passing => 1,
        seen => 2,
        skip => 0,
        todo => 0,
        'wait' => 0
    },
    simple => {
        bonus => 0,
        details => [
            ({
                actual_ok => 1,
                ok => 1
            }) x 5,
        ],
        'exit' => 0,
        max => 5,
        ok => 5,
        passing => 1,
        seen => 5,
        skip => 0,
        todo => 0,
        'wait' => 0
    },
    simple_fail => {
        bonus => 0,
        details => [
            {
                actual_ok => 1,
                ok => 1
            },
            {
                actual_ok => 0,
                ok => 0
            },
            {
                actual_ok => 1,
                ok => 1
            },
            {
                actual_ok => 1,
                ok => 1
            },
            {
                actual_ok => 0,
                ok => 0
            }
        ],
        'exit' => 0,
        max => 5,
        ok => 3,
        passing => 0,
        seen => 5,
        skip => 0,
        todo => 0,
        'wait' => 0
    },
    skip => {
        bonus => 0,
        details => [
            {
                actual_ok => 1,
                ok => 1
            },
            {
                actual_ok => 1,
                ok => 1,
                reason => "rain delay",
                type => "skip"
            },
            ({
                actual_ok => 1,
                ok => 1
            }) x 3,
        ],
        'exit' => 0,
        max => 5,
        ok => 5,
        passing => 1,
        seen => 5,
        skip => 1,
        todo => 0,
        'wait' => 0
    },
    skip_nomsg => {
        bonus => 0,
        details => [
            {
                actual_ok => 1,
                ok => 1,
                reason => "",
                type => "skip"
            }
        ],
        'exit' => 0,
        max => 1,
        ok => 1,
        passing => 1,
        seen => 1,
        skip => 1,
        todo => 0,
        'wait' => 0
    },
    skipall => {
        bonus => 0,
        details => [],
        'exit' => 0,
        max => 0,
        ok => 0,
        passing => 1,
        seen => 0,
        skip => 0,
        skip_all => "rope",
        todo => 0,
        'wait' => 0
    },
    skipall_nomsg => {
        bonus => 0,
        details => [],
        'exit' => 0,
        max => 0,
        ok => 0,
        passing => 1,
        seen => 0,
        skip => 0,
        skip_all => "",
        todo => 0,
        'wait' => 0
    },
    taint => {
        bonus => 0,
        details => [
            {
                actual_ok => 1,
                name => "-T honored",
                ok => 1
            }
        ],
        'exit' => 0,
        max => 1,
        ok => 1,
        passing => 1,
        seen => 1,
        skip => 0,
        todo => 0,
        'wait' => 0
    },
    todo => {
        bonus => 1,
        details => [
            {
                actual_ok => 1,
                ok => 1
            },
            {
                actual_ok => 1,
                ok => 1,
                type => "todo"
            },
            {
                actual_ok => 0,
                ok => 1,
                type => "todo"
            },
            ({
                actual_ok => 1,
                ok => 1
            }) x 2,
        ],
        'exit' => 0,
        max => 5,
        ok => 5,
        passing => 1,
        seen => 5,
        skip => 0,
        todo => 2,
        'wait' => 0
    },
    vms_nit => {
        bonus => 0,
        details => [
            {
                actual_ok => 0,
                ok => 0
            },
            {
                actual_ok => 1,
                ok => 1
            }
        ],
        'exit' => 0,
        max => 2,
        ok => 1,
        passing => 0,
        seen => 2,
        skip => 0,
        todo => 0,
        'wait' => 0
    },
    with_comments => {
        bonus => 2,
        details => [
            {
                actual_ok => 0,
                diagnostics => "Failed test 1 in t/todo.t at line 9 *TODO*\n",
                ok => 1,
                type => "todo"
            },
            {
                actual_ok => 1,
                ok => 1,
                reason => "at line 10 TODO?!)",
                type => "todo"
            },
            {
                actual_ok => 1,
                ok => 1
            },
            {
                actual_ok => 0,
                diagnostics => "Test 4 got: '0' (t/todo.t at line 12 *TODO*)\n  Expected: '1' (need more tuits)\n",
                ok => 1,
                type => "todo"
            },
            {
                actual_ok => 1,
                diagnostics => "woo\n",
                ok => 1,
                reason => "at line 13 TODO?!)",
                type => "todo"
            }
        ],
        'exit' => 0,
        max => 5,
        ok => 5,
        passing => 1,
        seen => 5,
        skip => 0,
        todo => 4,
        'wait' => 0
    },
);
plan tests => (keys(%samples) * 5) + 3;

use Test::Harness::Straps;
my @_INC = map { qq{"-I$_"} } @INC;
$Test::Harness::Switches = "@_INC -Mstrict";

$SIG{__WARN__} = sub { 
    warn @_ unless $_[0] =~ /^Enormous test number/ ||
                   $_[0] =~ /^Can't detailize/
};

for my $test ( sort keys %samples ) {
    print "# Working on $test\n";
    my $expect = $samples{$test};

    for my $n ( 0..$#{$expect->{details}} ) {
        for my $field ( qw( type name reason ) ) {
            $expect->{details}[$n]{$field} = '' unless exists $expect->{details}[$n]{$field};
        }
    }

    my $test_path = File::Spec->catfile($SAMPLE_TESTS, $test);
    my $strap = Test::Harness::Straps->new;
    isa_ok( $strap, 'Test::Harness::Straps' );
    my %results = $strap->analyze_file($test_path);

    is_deeply($results{details}, $expect->{details}, qq{details of "$test"} );

    delete $expect->{details};
    delete $results{details};

    SKIP: {
        skip '$? unreliable in MacPerl', 2 if $IsMacPerl;

        # We can only check if it's zero or non-zero.
        is( !!$results{'wait'}, !!$expect->{'wait'}, 'wait status' );
        delete $results{'wait'};
        delete $expect->{'wait'};

        # Have to check the exit status seperately so we can skip it
        # in MacPerl.
        is( $results{'exit'}, $expect->{'exit'} );
        delete $results{'exit'};
        delete $expect->{'exit'};
    }

    is_deeply(\%results, $expect, qq{ the rest of "$test"} );
} # for %samples

NON_EXISTENT_FILE: {
    my $strap = Test::Harness::Straps->new;
    isa_ok( $strap, 'Test::Harness::Straps' );
    ok( !$strap->analyze_file('I_dont_exist') );
    is( $strap->{error}, "I_dont_exist does not exist" );
}
