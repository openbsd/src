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

use Test::More;
use Test::Builder;
my $Test = Test::Builder->new;

$Test->plan( tests => 9 );
$Test->level(0);

my @Expected_Details;

$Test->is_num( scalar $Test->summary(), 0,   'no tests yet, no summary' );
push @Expected_Details, { 'ok'      => 1,
                          actual_ok => 1,
                          name      => 'no tests yet, no summary',
                          type      => '',
                          reason    => ''
                        };

# Inline TODO tests will confuse pre 1.20 Test::Harness, so we
# should just avoid the problem and not print it out.
my $out_fh  = $Test->output;
my $todo_fh = $Test->todo_output;
my $start_test = $Test->current_test + 1;
require TieOut;
tie *FH, 'TieOut';
$Test->output(\*FH);
$Test->todo_output(\*FH);

SKIP: {
    $Test->skip( 'just testing skip' );
}
push @Expected_Details, { 'ok'      => 1,
                          actual_ok => 1,
                          name      => '',
                          type      => 'skip',
                          reason    => 'just testing skip',
                        };

TODO: {
    local $TODO = 'i need a todo';
    $Test->ok( 0, 'a test to todo!' );

    push @Expected_Details, { 'ok'       => 1,
                              actual_ok  => 0,
                              name       => 'a test to todo!',
                              type       => 'todo',
                              reason     => 'i need a todo',
                            };

    $Test->todo_skip( 'i need both' );
}
push @Expected_Details, { 'ok'      => 1,
                          actual_ok => 0,
                          name      => '',
                          type      => 'todo_skip',
                          reason    => 'i need both'
                        };

for ($start_test..$Test->current_test) { print "ok $_\n" }
$Test->output($out_fh);
$Test->todo_output($todo_fh);

$Test->is_num( scalar $Test->summary(), 4,   'summary' );
push @Expected_Details, { 'ok'      => 1,
                          actual_ok => 1,
                          name      => 'summary',
                          type      => '',
                          reason    => '',
                        };

$Test->current_test(6);
print "ok 6 - current_test incremented\n";
push @Expected_Details, { 'ok'      => 1,
                          actual_ok => undef,
                          name      => undef,
                          type      => 'unknown',
                          reason    => 'incrementing test number',
                        };

my @details = $Test->details();
$Test->is_num( scalar @details, 6,
    'details() should return a list of all test details');

$Test->level(1);
is_deeply( \@details, \@Expected_Details );


# This test has to come last because it thrashes the test details.
{
    my $curr_test = $Test->current_test;
    $Test->current_test(4);
    my @details = $Test->details();

    $Test->current_test($curr_test);
    $Test->is_num( scalar @details, 4 );
}
