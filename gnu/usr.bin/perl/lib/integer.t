#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use integer;

use Test::More tests => 11;
use Config;

my $x = 4.5;
my $y = 5.6;
my $z;

$z = $x + $y;
is($z, 9, "plus");

$z = $x - $y;
is($z, -1, "minus");

$z = $x * $y;
is($z, 20, "times");

$z = $x / $y;
is($z, 0, "divide");

$z = $x / $y;
is($z, 0, "modulo");

is($x, 4.5, "scalar still floating point");
 
isnt(sqrt($x), 2, "functions still floating point");
 
isnt($x ** .5, 2, "power still floating point");

is(++$x, 5.5, "++ still floating point");
 
SKIP: {
    my $ivsize = $Config{ivsize};
    skip "ivsize == $ivsize", 2 unless $ivsize == 4 || $ivsize == 8;

    if ($ivsize == 4) {
	$z = 2**31 - 1;
	is($z + 1, -2147483648, "left shift");
    } elsif ($ivsize == 8) {
	$z = 2**63 - 1;
	is($z + 1, -9223372036854775808, "left shift");
    }
}

is(~0, -1, "signed instead of unsigned");
