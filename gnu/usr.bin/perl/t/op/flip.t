#!./perl

chdir 't' if -d 't';

print "1..15\n";

@a = (1,2,3,4,5,6,7,8,9,10,11,12);

while ($_ = shift(@a)) {
    if ($x = /4/../8/) { $z = $x; print "ok ", $x + 0, "\n"; }
    $y .= /1/../2/;
}

if ($z eq '5E0') {print "ok 6\n";} else {print "not ok 6\n";}

if ($y eq '12E0123E0') {print "ok 7\n";} else {print "not ok 7\n";}

@a = ('a','b','c','d','e','f','g');

{
local $.;

open(of,'harness') or die "Can't open harness: $!";
while (<of>) {
    (3 .. 5) && ($foo .= $_);
}
$x = ($foo =~ y/\n/\n/);

if ($x eq 3) {print "ok 8\n";} else {print "not ok 8 $x:$foo:\n";}

$x = 3.14;
if (($x...$x) eq "1") {print "ok 9\n";} else {print "not ok 9\n";}

{
    # coredump reported in bug 20001018.008
    readline(UNKNOWN);
    $. = 1;
    $x = 1..10;
    print "ok 10\n";
}

}

if (!defined $.) { print "ok 11\n" } else { print "not ok 11 # $.\n" }

use warnings;
my $warn='';
$SIG{__WARN__} = sub { $warn .= join '', @_ };

if (0..2) { print "ok 12\n" } else { print "not ok 12\n" }

if ($warn =~ /uninitialized/) { print "ok 13\n" } else { print "not ok 13\n" }
$warn = '';

$x = "foo".."bar";

if ((() = ($warn =~ /isn't numeric/g)) == 2) {
    print "ok 14\n"
}
else {
    print "not ok 14\n"
}
$warn = '';

$. = 15;
if (15..0) { print "ok 15\n" } else { print "not ok 15\n" }
