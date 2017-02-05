#!./perl

use strict;
use warnings;

use Test::More tests => 6;

use List::Util qw(shuffle);

my @r;

@r = shuffle();
ok( !@r,	'no args');

@r = shuffle(9);
is( 0+@r,	1,	'1 in 1 out');
is( $r[0],	9,	'one arg');

my @in = 1..100;
@r = shuffle(@in);
is( 0+@r,	0+@in,	'arg count');

isnt( "@r",	"@in",	'result different to args');

my @s = sort { $a <=> $b } @r;
is( "@in",	"@s",	'values');
