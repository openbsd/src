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
   combined   => {
                  passing     => 0,

                  'exit'      => 0,
                  'wait'      => 0,

                  max         => 10,
                  seen        => 10,

                  'ok'        => 8,
                  'todo'      => 2,
                  'skip'      => 1,
                  bonus       => 1,

                  details     => [ { 'ok' => 1, actual_ok => 1 },
                                   { 'ok' => 1, actual_ok => 1,
                                     name => 'basset hounds got long ears',
                                   },
                                   { 'ok' => 0, actual_ok => 0,
                                     name => 'all hell broke lose',
                                   },
                                   { 'ok' => 1, actual_ok => 1,
                                     type => 'todo'
                                   },
                                   { 'ok' => 1, actual_ok => 1 },
                                   { 'ok' => 1, actual_ok => 1 },
                                   { 'ok' => 1, actual_ok => 1,
                                     type   => 'skip',
                                     reason => 'contract negociations'
                                   },
                                   { 'ok' => 1, actual_ok => 1 },
                                   { 'ok' => 0, actual_ok => 0 },
                                   { 'ok' => 1, actual_ok => 0,
                                     type   => 'todo' 
                                   },
                                 ]
                       },

   descriptive      => {
                        passing     => 1,

                        'wait'      => 0,
                        'exit'      => 0,

                        max         => 5,
                        seen        => 5,

                        'ok'          => 5,
                        'todo'        => 0,
                        'skip'        => 0,
                        bonus       => 0,

                        details     => [ { 'ok' => 1, actual_ok => 1,
                                           name => 'Interlock activated'
                                         },
                                         { 'ok' => 1, actual_ok => 1,
                                           name => 'Megathrusters are go',
                                         },
                                         { 'ok' => 1, actual_ok => 1,
                                           name => 'Head formed',
                                         },
                                         { 'ok' => 1, actual_ok => 1,
                                           name => 'Blazing sword formed'
                                         },
                                         { 'ok' => 1, actual_ok => 1,
                                           name => 'Robeast destroyed'
                                         },
                                       ],
                       },

   duplicates       => {
                        passing     => 0,

                        'exit'      => 0,
                        'wait'      => 0,

                        max         => 10,
                        seen        => 11,

                        'ok'          => 11,
                        'todo'        => 0,
                        'skip'        => 0,
                        bonus       => 0,

                        details     => [ ({ 'ok' => 1, actual_ok => 1 }) x 10
                                       ],
                       },

   head_end         => {
                        passing     => 1,

                        'exit'      => 0,
                        'wait'      => 0,

                        max         => 4,
                        seen        => 4,

                        'ok'        => 4,
                        'todo'      => 0,
                        'skip'      => 0,
                        bonus       => 0,

                        details     => [ ({ 'ok' => 1, actual_ok => 1 }) x 4
                                       ],
                       },

   lone_not_bug     => {
                        passing     => 1,

                        'exit'      => 0,
                        'wait'      => 0,

                        max         => 4,
                        seen        => 4,

                        'ok'        => 4,
                        'todo'      => 0,
                        'skip'      => 0,
                        bonus       => 0,

                        details     => [ ({ 'ok' => 1, actual_ok => 1 }) x 4
                                       ],
                       },

   head_fail           => {
                           passing  => 0,

                           'exit'   => 0,
                           'wait'   => 0,

                           max      => 4,
                           seen     => 4,

                           'ok'     => 3,
                           'todo'   => 0,
                           'skip'   => 0,
                           bonus    => 0,

                           details  => [ { 'ok' => 1, actual_ok => 1 },
                                         { 'ok' => 0, actual_ok => 0 },
                                         ({ 'ok'=> 1, actual_ok => 1 }) x 2
                                       ],
                          },

   no_output        => {
                        passing     => 0,

                        'exit'      => 0,
                        'wait'      => 0,

                        max         => 0,
                        seen        => 0,

                        'ok'        => 0,
                        'todo'      => 0,
                        'skip'      => 0,
                        bonus       => 0,

                        details     => [],
                       },

   simple           => {
                        passing     => 1,

                        'exit'      => 0,
                        'wait'      => 0,

                        max         => 5,
                        seen        => 5,

                        'ok'          => 5,
                        'todo'        => 0,
                        'skip'        => 0,
                        bonus       => 0,

                        details     => [ ({ 'ok' => 1, actual_ok => 1 }) x 5
                                       ]
                       },

   simple_fail      => {
                        passing     => 0,

                        'exit'      => 0,
                        'wait'      => 0,

                        max         => 5,
                        seen        => 5,

                        'ok'          => 3,
                        'todo'        => 0,
                        'skip'        => 0,
                        bonus       => 0,

                        details     => [ { 'ok' => 1, actual_ok => 1 },
                                         { 'ok' => 0, actual_ok => 0 },
                                         { 'ok' => 1, actual_ok => 1 },
                                         { 'ok' => 1, actual_ok => 1 },
                                         { 'ok' => 0, actual_ok => 0 },
                                       ]
                       },

   'skip'             => {
                        passing     => 1,

                        'exit'      => 0,
                        'wait'      => 0,

                        max         => 5,
                        seen        => 5,

                        'ok'          => 5,
                        'todo'        => 0,
                        'skip'        => 1,
                        bonus       => 0,

                        details     => [ { 'ok' => 1, actual_ok => 1 },
                                         { 'ok'   => 1, actual_ok => 1,
                                           type   => 'skip',
                                           reason => 'rain delay',
                                         },
                                         ({ 'ok' => 1, actual_ok => 1 }) x 3
                                       ]
                       },

   'skip_nomsg'     => {
                        passing     => 1,

                        'exit'      => 0,
                        'wait'      => 0,

                        max         => 1,
                        seen        => 1,

                        'ok'          => 1,
                        'todo'        => 0,
                        'skip'        => 1,
                        bonus       => 0,

                        details     => [ { 'ok'   => 1, actual_ok => 1,
                                           type   => 'skip',
                                           reason => '',
                                         },
                                       ]
                       },

   skipall           => {
                          passing   => 1,

                          'exit'    => 0,
                          'wait'    => 0,

                          max       => 0,
                          seen      => 0,
                          skip_all  => 'rope',

                          'ok'      => 0,
                          'todo'    => 0,
                          'skip'    => 0,
                          bonus     => 0,

                          details   => [],
                         },

   skipall_nomsg    => {
                          passing   => 1,

                          'exit'    => 0,
                          'wait'    => 0,

                          max       => 0,
                          seen      => 0,
                          skip_all  => '',

                          'ok'      => 0,
                          'todo'    => 0,
                          'skip'    => 0,
                          bonus     => 0,

                          details   => [],
                         },

   'todo'             => {
                        passing     => 1,

                        'exit'      => 0,
                        'wait'      => 0,

                        max         => 5,
                        seen        => 5,

                        'ok'          => 5,
                        'todo'        => 2,
                        'skip'        => 0,
                        bonus       => 1,

                        details     => [ { 'ok' => 1, actual_ok => 1 },
                                         { 'ok' => 1, actual_ok => 1,
                                           type => 'todo' },
                                         { 'ok' => 1, actual_ok => 0,
                                           type => 'todo' },
                                         ({ 'ok' => 1, actual_ok => 1 }) x 2
                                       ],
                       },
   taint            => {
                        passing     => 1,

                        'exit'      => 0,
                        'wait'      => 0,

                        max         => 1,
                        seen        => 1,

                        'ok'          => 1,
                        'todo'        => 0,
                        'skip'        => 0,
                        bonus       => 0,

                        details     => [ { 'ok' => 1, actual_ok => 1,
                                           name => '- -T honored'
                                         },
                                       ],
                       },
   vms_nit          => {
                        passing     => 0,

                        'exit'      => 0,
                        'wait'      => 0,

                        max         => 2,
                        seen        => 2,

                        'ok'          => 1,
                        'todo'        => 0,
                        'skip'        => 0,
                        bonus       => 0,

                        details     => [ { 'ok' => 0, actual_ok => 0 },
                                         { 'ok' => 1, actual_ok => 1 },
                                       ],
                       },
   'die'            => {
                        passing     => 0,

                        'exit'      => $die_exit,
                        'wait'      => $wait_non_zero,

                        max         => 0,
                        seen        => 0,

                        'ok'        => 0,
                        'todo'      => 0,
                        'skip'      => 0,
                        bonus       => 0,

                        details     => []
                       },

   die_head_end     => {
                        passing     => 0,

                        'exit'      => $die_exit,
                        'wait'      => $wait_non_zero,

                        max         => 0,
                        seen        => 4,

                        'ok'        => 4,
                        'todo'      => 0,
                        'skip'      => 0,
                        bonus       => 0,

                        details     => [ ({ 'ok' => 1, actual_ok => 1 }) x 4
                                       ],
                       },

   die_last_minute  => {
                        passing     => 0,

                        'exit'      => $die_exit,
                        'wait'      => $wait_non_zero,

                        max         => 4,
                        seen        => 4,

                        'ok'        => 4,
                        'todo'      => 0,
                        'skip'      => 0,
                        bonus       => 0,

                        details     => [ ({ 'ok' => 1, actual_ok => 1 }) x 4
                                       ],
                       },

   bignum           => {
                        passing     => 0,

                        'exit'      => 0,
                        'wait'      => 0,

                        max         => 2,
                        seen        => 4,

                        'ok'          => 4,
                        'todo'        => 0,
                        'skip'        => 0,
                        bonus       => 0,

                        details     => [ { 'ok' => 1, actual_ok => 1 },
                                         { 'ok' => 1, actual_ok => 1 },
                                       ]
                       },

   'shbang_misparse' =>{
                        passing     => 1,

                        'exit'      => 0,
                        'wait'      => 0,

                        max         => 2,
                        seen        => 2,

                        'ok'          => 2,
                        'todo'        => 0,
                        'skip'        => 0,
                        bonus       => 0,

                        details     => [ ({ 'ok' => 1, actual_ok => 1 }) x 2 ]
                       },
);

plan tests => (keys(%samples) * 5) + 4;

use_ok('Test::Harness::Straps');

$SIG{__WARN__} = sub { 
    warn @_ unless $_[0] =~ /^Enormous test number/ ||
                   $_[0] =~ /^Can't detailize/
};
while( my($test, $expect) = each %samples ) {
    for (0..$#{$expect->{details}}) {
        $expect->{details}[$_]{type} = ''
            unless exists $expect->{details}[$_]{type};
        $expect->{details}[$_]{name} = ''
            unless exists $expect->{details}[$_]{name};
        $expect->{details}[$_]{reason} = ''
            unless exists $expect->{details}[$_]{reason};
    }

    my $test_path = File::Spec->catfile($SAMPLE_TESTS, $test);
    my $strap = Test::Harness::Straps->new;
    isa_ok( $strap, 'Test::Harness::Straps' );
    my %results = $strap->analyze_file($test_path);

    is_deeply($results{details}, $expect->{details}, "$test details" );

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

    is_deeply(\%results, $expect, "  the rest $test" );
}


my $strap = Test::Harness::Straps->new;
isa_ok( $strap, 'Test::Harness::Straps' );
ok( !$strap->analyze_file('I_dont_exist') );
is( $strap->{error}, "I_dont_exist does not exist" );
