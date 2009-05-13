#!perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}


# Turn on threads here, if available, since this test tends to find
# lots of threading bugs.
use Config;
BEGIN {
    if( $] >= 5.008001 && $Config{useithreads} ) {
        require threads;
        'threads'->import;
    }
}


use strict;

use Test::More tests => 7;

my $test = Test::Builder->create;

# now make a filehandle where we can send data
use TieOut;
my $output = tie *FAKEOUT, 'TieOut';


# Test diag() goes to todo_output() in a todo test.
{
    $test->todo_start();
    $test->todo_output(\*FAKEOUT);

    $test->diag("a single line");
    is( $output->read, <<'DIAG',   'diag() with todo_output set' );
# a single line
DIAG

    my $ret = $test->diag("multiple\n", "lines");
    is( $output->read, <<'DIAG',   '  multi line' );
# multiple
# lines
DIAG
    ok( !$ret, 'diag returns false' );

    $test->todo_end();
}

$test->reset_outputs();


# Test diagnostic formatting
$test->failure_output(\*FAKEOUT);
{
    $test->diag("# foo");
    is( $output->read, "# # foo\n", "diag() adds # even if there's one already" );

    $test->diag("foo\n\nbar");
    is( $output->read, <<'DIAG', "  blank lines get escaped" );
# foo
# 
# bar
DIAG


    $test->diag("foo\n\nbar\n\n");
    is( $output->read, <<'DIAG', "  even at the end" );
# foo
# 
# bar
# 
DIAG
}


# [rt.cpan.org 8392]
{
    $test->diag(qw(one two));
}
is( $output->read, <<'DIAG' );
# onetwo
DIAG
