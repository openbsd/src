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
use File::Spec;

my $Curdir = File::Spec->curdir;
my $SAMPLE_TESTS = $ENV{PERL_CORE}
                    ? File::Spec->catdir($Curdir, 'lib', 'sample-tests')
                    : File::Spec->catdir($Curdir, 't',   'sample-tests');


use Test::More;
use Dev::Null;

my $IsMacPerl = $^O eq 'MacOS';
my $IsVMS     = $^O eq 'VMS';

# VMS uses native, not POSIX, exit codes.
# MacPerl's exit codes are broken.
my $die_estat = $IsVMS     ? 44 : 
                $IsMacPerl ? 0  :
                             1;

my %samples = (
            simple            => {
                                  total => {
                                            bonus      => 0,
                                            max        => 5,
                                            'ok'       => 5,
                                            files      => 1,
                                            bad        => 0,
                                            good       => 1,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => { },
                                  all_ok => 1,
                                 },
            simple_fail      => {
                                 total => {
                                           bonus       => 0,
                                           max         => 5,
                                           'ok'        => 3,
                                           files       => 1,
                                           bad         => 1,
                                           good        => 0,
                                           tests       => 1,
                                           sub_skipped => 0,
                                           'todo'      => 0,
                                           skipped     => 0,
                                          },
                                 failed => {
                                            canon      => '2 5',
                                           },
                                 all_ok => 0,
                                },
            descriptive       => {
                                  total => {
                                            bonus      => 0,
                                            max        => 5,
                                            'ok'       => 5,
                                            files      => 1,
                                            bad        => 0,
                                            good       => 1,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => { },
                                  all_ok => 1,
                                 },
            no_nums           => {
                                  total => {
                                            bonus      => 0,
                                            max        => 5,
                                            'ok'       => 4,
                                            files      => 1,
                                            bad        => 1,
                                            good       => 0,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => {
                                             canon     => '3',
                                            },
                                  all_ok => 0,
                                 },
            'todo'            => {
                                  total => {
                                            bonus      => 1,
                                            max        => 5,
                                            'ok'       => 5,
                                            files      => 1,
                                            bad        => 0,
                                            good       => 1,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 2,
                                            skipped    => 0,
                                           },
                                  failed => { },
                                  all_ok => 1,
                                 },
            todo_inline       => {
                                  total => {
                                            bonus       => 1,
                                            max         => 3,
                                            'ok'        => 3,
                                            files       => 1,
                                            bad         => 0,
                                            good        => 1,
                                            tests       => 1,
                                            sub_skipped => 0,
                                            'todo'      => 2,
                                            skipped     => 0,
                                           },
                                  failed => { },
                                  all_ok => 1,
                                 },
            'skip'            => {
                                  total => {
                                            bonus      => 0,
                                            max        => 5,
                                            'ok'       => 5,
                                            files      => 1,
                                            bad        => 0,
                                            good       => 1,
                                            tests      => 1,
                                            sub_skipped=> 1,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => { },
                                  all_ok => 1,
                                 },
            'skip_nomsg'      => {
                                  total => {
                                            bonus      => 0,
                                            max        => 1,
                                            'ok'       => 1,
                                            files      => 1,
                                            bad        => 0,
                                            good       => 1,
                                            tests      => 1,
                                            sub_skipped=> 1,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => { },
                                  all_ok => 1,
                                 },
            bailout           => 0,
            combined          => {
                                  total => {
                                            bonus      => 1,
                                            max        => 10,
                                            'ok'       => 8,
                                            files      => 1,
                                            bad        => 1,
                                            good       => 0,
                                            tests      => 1,
                                            sub_skipped=> 1,
                                            'todo'     => 2,
                                            skipped    => 0
                                           },
                                  failed => {
                                             canon     => '3 9',
                                            },
                                  all_ok => 0,
                                 },
            duplicates        => {
                                  total => {
                                            bonus      => 0,
                                            max        => 10,
                                            'ok'       => 11,
                                            files      => 1,
                                            bad        => 1,
                                            good       => 0,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => {
                                             canon     => '??',
                                            },
                                  all_ok => 0,
                                 },
            head_end          => {
                                  total => {
                                            bonus      => 0,
                                            max        => 4,
                                            'ok'       => 4,
                                            files      => 1,
                                            bad        => 0,
                                            good       => 1,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => { },
                                  all_ok => 1,
                                 },
            head_fail         => {
                                  total => {
                                            bonus      => 0,
                                            max        => 4,
                                            'ok'       => 3,
                                            files      => 1,
                                            bad        => 1,
                                            good       => 0,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => {
                                             canon      => '2',
                                            },
                                  all_ok => 0,
                                 },
            no_output        => {
                                 total => {
                                           bonus       => 0,
                                           max         => 0,
                                           'ok'        => 0,
                                           files       => 1,
                                           bad         => 1,
                                           good        => 0,
                                           tests       => 1,
                                           sub_skipped => 0,
                                           'todo'      => 0,
                                           skipped     => 0,
                                          },
                                 failed => {
                                           },
                                 all_ok => 0,
                                },
            skipall          => {
                                  total => {
                                            bonus      => 0,
                                            max        => 0,
                                            'ok'       => 0,
                                            files      => 1,
                                            bad        => 0,
                                            good       => 1,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 1,
                                           },
                                  failed => { },
                                  all_ok => 1,
                                 },
            skipall_nomsg   => {
                                  total => {
                                            bonus      => 0,
                                            max        => 0,
                                            'ok'       => 0,
                                            files      => 1,
                                            bad        => 0,
                                            good       => 1,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 1,
                                           },
                                  failed => { },
                                  all_ok => 1,
                                 },
            with_comments     => {
                                  total => {
                                            bonus      => 2,
                                            max        => 5,
                                            'ok'       => 5,
                                            files      => 1,
                                            bad        => 0,
                                            good       => 1,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 4,
                                            skipped    => 0,
                                           },
                                  failed => { },
                                  all_ok => 1,
                                 },
            taint             => {
                                  total => {
                                            bonus      => 0,
                                            max        => 1,
                                            'ok'       => 1,
                                            files      => 1,
                                            bad        => 0,
                                            good       => 1,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => { },
                                  all_ok => 1,
                                 },

            taint_warn        => {
                                  total => {
                                            bonus      => 0,
                                            max        => 1,
                                            'ok'       => 1,
                                            files      => 1,
                                            bad        => 0,
                                            good       => 1,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => { },
                                  all_ok => 1,
                                 },

            'die'             => {
                                  total => {
                                            bonus      => 0,
                                            max        => 0,
                                            'ok'       => 0,
                                            files      => 1,
                                            bad        => 1,
                                            good       => 0,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => {
                                             estat      => $die_estat,
                                             max        => '??',
                                             failed     => '??',
                                             canon      => '??',
                                            },
                                  all_ok => 0,
                                 },

            die_head_end      => {
                                  total => {
                                            bonus      => 0,
                                            max        => 0,
                                            'ok'       => 4,
                                            files      => 1,
                                            bad        => 1,
                                            good       => 0,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => {
                                             estat      => $die_estat,
                                             max        => '??',
                                             failed     => '??',
                                             canon      => '??',
                                            },
                                  all_ok => 0,
                                 },

            die_last_minute   => {
                                  total => {
                                            bonus      => 0,
                                            max        => 4,
                                            'ok'       => 4,
                                            files      => 1,
                                            bad        => 1,
                                            good       => 0,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => {
                                             estat      => $die_estat,
                                             max        => 4,
                                             failed     => 0,
                                             canon      => '??',
                                            },
                                  all_ok => 0,
                                 },
            bignum            => {
                                  total => {
                                            bonus      => 0,
                                            max        => 2,
                                            'ok'       => 4,
                                            files      => 1,
                                            bad        => 1,
                                            good       => 0,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => {
                                             canon      => '??',
                                            },
                                  all_ok => 0,
                                 },
            bignum_many       => {
                                  total => {
                                            bonus      => 0,
                                            max        => 2,
                                            'ok'       => 11,
                                            files      => 1,
                                            bad        => 1,
                                            good       => 0,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => {
                                             canon      => '3-100000',
                                            },
                                  all_ok => 0,
                                 },
            'shbang_misparse' => {
                                  total => {
                                            bonus      => 0,
                                            max        => 2,
                                            'ok'       => 2,
                                            files      => 1,
                                            bad        => 0,
                                            good       => 1,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => { },
                                  all_ok => 1,
                                 },
            too_many         => {
                                 total => {
                                           bonus       => 0,
                                           max         => 3,
                                           'ok'        => 7,
                                           files       => 1,
                                           bad         => 1,
                                           good        => 0,
                                           tests       => 1,
                                           sub_skipped => 0,
                                           'todo'      => 0,
                                           skipped     => 0,
                                          },
                                 failed => {
                                            canon      => '4-7',
                                           },
                                 all_ok => 0,
                                },
            switches         => {
                                  total => {
                                            bonus      => 0,
                                            max        => 1,
                                            'ok'       => 1,
                                            files      => 1,
                                            bad        => 0,
                                            good       => 1,
                                            tests      => 1,
                                            sub_skipped=> 0,
                                            'todo'     => 0,
                                            skipped    => 0,
                                           },
                                  failed => { },
                                  all_ok => 1,
                                 },
           );

plan tests => (keys(%samples) * 7);

use Test::Harness;
my @_INC = map { qq{"-I$_"} } @INC;
$Test::Harness::Switches = "@_INC -Mstrict";

tie *NULL, 'Dev::Null' or die $!;

for my $test ( sort keys %samples ) {
SKIP: {
    skip "-t introduced in 5.8.0", 7 if $test eq 'taint_warn' and $] < 5.008;

    my $expect = $samples{$test};

    # _run_all_tests() runs the tests but skips the formatting.
    my($totals, $failed);
    my $warning = '';
    my $test_path = File::Spec->catfile($SAMPLE_TESTS, $test);

    print STDERR "# $test\n" if $ENV{TEST_VERBOSE};
    eval {
        select NULL;    # _run_all_tests() isn't as quiet as it should be.
        local $SIG{__WARN__} = sub { $warning .= join '', @_; };
        ($totals, $failed) = 
          Test::Harness::_run_all_tests($test_path);
    };
    select STDOUT;

    # $? is unreliable in MacPerl, so we'll just fudge it.
    $failed->{estat} = $die_estat if $IsMacPerl and $failed;

    SKIP: {
        skip "special tests for bailout", 1 unless $test eq 'bailout';
        like( $@, '/Further testing stopped: GERONI/i' );
    }

    SKIP: {
        skip "don't apply to a bailout", 5 if $test eq 'bailout';
        is( $@, '' );
        is( Test::Harness::_all_ok($totals), $expect->{all_ok},
                                                  "$test - all ok" );
        ok( defined $expect->{total},             "$test - has total" );
        is_deeply( {map { $_=>$totals->{$_} } keys %{$expect->{total}}},
                   $expect->{total},
                                                  "$test - totals" );
        is_deeply( {map { $_=>$failed->{$test_path}{$_} }
                    keys %{$expect->{failed}}},
                   $expect->{failed},
                                                  "$test - failed" );
    }

    my $expected_warnings = "";
    if ( $test eq "bignum" ) {
        $expected_warnings = <<WARN;
Enormous test number seen [test 136211425]
Can't detailize, too big.
WARN
    }
    elsif ( $test eq 'bignum_many' ) {
        $expected_warnings = <<WARN;
Enormous test number seen [test 100001]
Can't detailize, too big.
WARN
    }
    my $desc = $expected_warnings ? 'Got proper warnings' : 'No warnings';
    is( $warning, $expected_warnings, "$test - $desc" );
} # taint SKIP block
} # for tests
