#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan 15;

# compile time evaluation

if (int(1.234) == 1) {pass()} else {fail()}

if (int(-1.234) == -1) {pass()} else {fail()}

# run time evaluation

$x = 1.234;
cmp_ok(int($x), '==', 1);
cmp_ok(int(-$x), '==', -1);

$x = length("abc") % -10;
cmp_ok($x, '==', -7);

{
    my $fail;
    use integer;
    $x = length("abc") % -10;
    $y = (3/-10)*-10;
    ok($x+$y == 3) or ++$fail;
    ok(abs($x) < 10) or ++$fail;
    if ($fail) {
	diag("\$x == $x", "\$y == $y");
    }
}

@x = ( 6, 8, 10);
cmp_ok($x["1foo"], '==', 8, 'check bad strings still get converted');

$x = 4294967303.15;
$y = int ($x);
is($y, "4294967303", 'check values > 32 bits work');

$y = int (-$x);

is($y, "-4294967303");

$x = 4294967294.2;
$y = int ($x);

is($y, "4294967294");

$x = 4294967295.7;
$y = int ($x);

is($y, "4294967295");

$x = 4294967296.11312;
$y = int ($x);

is($y, "4294967296");

$y = int(279964589018079/59);
cmp_ok($y, '==', 4745162525730);

$y = 279964589018079;
$y = int($y/59);
cmp_ok($y, '==', 4745162525730);
