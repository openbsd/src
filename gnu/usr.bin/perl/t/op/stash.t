#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib);
}

require "./test.pl";

plan( tests => 2 );

# Used to segfault (bug #15479)
fresh_perl_is(
    '%:: = ""',
    'Odd number of elements in hash assignment at - line 1.',
    { switches => [ '-w' ] },
    'delete $::{STDERR} and print a warning',
);

# Used to segfault
fresh_perl_is(
    'BEGIN { $::{"X::"} = 2 }',
    '',
    { switches => [ '-w' ] },
    q(Insert a non-GV in a stash, under warnings 'once'),
);
