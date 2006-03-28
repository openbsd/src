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
    if( $] >= 5.008 && $Config{useithreads} ) {
        require threads;
        'threads'->import;
    }
}


use strict;

use Test::More tests => 5;

my $Test = Test::More->builder;

# now make a filehandle where we can send data
use TieOut;
my $output = tie *FAKEOUT, 'TieOut';

# force diagnostic output to a filehandle, glad I added this to
# Test::Builder :)
my $ret;
{
    local $TODO = 1;
    $Test->todo_output(\*FAKEOUT);

    diag("a single line");

    $ret = diag("multiple\n", "lines");
}

is( $output->read, <<'DIAG',   'diag() with todo_output set' );
# a single line
# multiple
# lines
DIAG

ok( !$ret, 'diag returns false' );

{
    $Test->failure_output(\*FAKEOUT);
    $ret = diag("# foo");
}
$Test->failure_output(\*STDERR);
is( $output->read, "# # foo\n", "diag() adds # even if there's one already" );
ok( !$ret,  'diag returns false' );


# [rt.cpan.org 8392]
{
    $Test->failure_output(\*FAKEOUT);
    diag(qw(one two));
}
$Test->failure_output(\*STDERR);
is( $output->read, <<'DIAG' );
# onetwo
DIAG
