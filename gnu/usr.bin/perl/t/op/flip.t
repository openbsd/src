#!./perl

BEGIN {
    require "test.pl";
}

plan(11);

@a = (1,2,3,4,5,6,7,8,9,10,11,12);
@b = ();
while ($_ = shift(@a)) {
    if ($x = /4/../8/) { $z = $x; push @b, $x + 0; }
    $y .= /1/../2/;
}
is(join("*", @b), "1*2*3*4*5");

is($z, '5E0');

is($y, '12E0123E0');

@a = ('a','b','c','d','e','f','g');

{
local $.;

open(of,'harness') or die "Can't open harness: $!";
while (<of>) {
    (3 .. 5) && ($foo .= $_);
}
$x = ($foo =~ y/\n/\n/);

is($x, 3);

$x = 3.14;
ok(($x...$x) eq "1");

{
    # coredump reported in bug 20001018.008
    readline(UNKNOWN);
    $. = 1;
    $x = 1..10;
    ok(1);
}

}

ok(!defined $.);

use warnings;
my $warn='';
$SIG{__WARN__} = sub { $warn .= join '', @_ };

ok(scalar(0..2));

like($warn, qr/uninitialized/);
$warn = '';

$x = "foo".."bar";

ok((() = ($warn =~ /isn't numeric/g)) == 2);
$warn = '';

$. = 15;
ok(scalar(15..0));
