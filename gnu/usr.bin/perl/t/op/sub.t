#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan( tests => 4 );

sub empty_sub {}

is(empty_sub,undef,"Is empty");
is(empty_sub(1,2,3),undef,"Is still empty");
@test = empty_sub();
is(scalar(@test), 0, 'Didnt return anything');
@test = empty_sub(1,2,3);
is(scalar(@test), 0, 'Didnt return anything');

