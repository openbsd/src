#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}
use strict;
use warnings;
use vars qw(@array @r $k $v $c);

plan tests => 57;

@array = qw(crunch zam bloop);

(@r) = each @array;
is (scalar @r, 2);
is ($r[0], 0);
is ($r[1], 'crunch');
($k, $v) = each @array;
is ($k, 1);
is ($v, 'zam');
($k, $v) = each @array;
is ($k, 2);
is ($v, 'bloop');
(@r) = each @array;
is (scalar @r, 0);

(@r) = each @array;
is (scalar @r, 2);
is ($r[0], 0);
is ($r[1], 'crunch');
($k) = each @array;
is ($k, 1);

my @lex_array = qw(PLOP SKLIZZORCH RATTLE);

(@r) = each @lex_array;
is (scalar @r, 2);
is ($r[0], 0);
is ($r[1], 'PLOP');
($k, $v) = each @lex_array;
is ($k, 1);
is ($v, 'SKLIZZORCH');
($k) = each @lex_array;
is ($k, 2);
(@r) = each @lex_array;
is (scalar @r, 0);

my $ar = ['bacon'];

(@r) = each @$ar;
is (scalar @r, 2);
is ($r[0], 0);
is ($r[1], 'bacon');

(@r) = each @$ar;
is (scalar @r, 0);

is (each @$ar, 0);
is (scalar each @$ar, undef);

my @keys;
@keys = keys @array;
is ("@keys", "0 1 2");

@keys = keys @lex_array;
is ("@keys", "0 1 2");

($k, $v) = each @array;
is ($k, 0);
is ($v, 'crunch');

@keys = keys @array;
is ("@keys", "0 1 2");

($k, $v) = each @array;
is ($k, 0);
is ($v, 'crunch');



my @values;
@values = values @array;
is ("@values", "@array");

@values = values @lex_array;
is ("@values", "@lex_array");

($k, $v) = each @array;
is ($k, 0);
is ($v, 'crunch');

@values = values @array;
is ("@values", "@array");

($k, $v) = each @array;
is ($k, 0);
is ($v, 'crunch');

# reset
while (each @array) { }

# each(ARRAY) in the conditional loop
$c = 0;
while (($k, $v) = each @array) {
    is ($k, $c);
    is ($v, $array[$k]);
    $c++;
}

# each(ARRAY) on scalar context in conditional loop
# should guarantee to be wrapped into defined() function.
# first return value will be 0 --> [#90888]
$c = 0;
$k = 0;
$v = 0;
while ($k = each @array) {
    is ($k, $v);
    $v++;
}

# each(ARRAY) in the conditional loop
$c = 0;
for (; ($k, $v) = each @array ;) {
    is ($k, $c);
    is ($v, $array[$k]);
    $c++;
}

# each(ARRAY) on scalar context in conditional loop
# --> [#90888]
$c = 0;
$k = 0;
$v = 0;
for (; $k = each(@array) ;) {
    is ($k, $v);
    $v++;
}
