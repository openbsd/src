#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}   

plan tests => 2;

is(
    sprintf("%.40g ",0.01),
    sprintf("%.40g", 0.01)." ",
    q(the sprintf "%.<number>g" optimization)
);
is(
    sprintf("%.40f ",0.01),
    sprintf("%.40f", 0.01)." ",
    q(the sprintf "%.<number>f" optimization)
);
