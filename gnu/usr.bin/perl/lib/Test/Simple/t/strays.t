#!/usr/bin/perl -w 

# Check that stray newlines in test output are probably handed.

BEGIN {
    print "1..0 # Skip not completed\n";
    exit 0;
}

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}
chdir 't';

use TieOut;
local *FAKEOUT;
my $out = tie *FAKEOUT, 'TieOut';


use Test::Builder;
my $Test = Test::Builder->new;
my $orig_out  = $Test->output;
my $orig_err  = $Test->failure_output;
my $orig_todo = $Test->todo_output;

$Test->output(\*FAKEOUT);
$Test->failure_output(\*FAKEOUT);
$Test->todo_output(\*FAKEOUT);
$Test->no_plan();

$Test->ok(1, "name\n");
$Test->ok(0, "foo\nbar\nbaz");
$Test->skip("\nmoofer");
$Test->todo_skip("foo\n\n");

