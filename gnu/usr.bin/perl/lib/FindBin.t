#!./perl

BEGIN {
    # Can't chdir in BEGIN before FindBin runs, as it then can't find us.
    @INC = -d 't' ? 'lib' : '../lib';
}

print "1..2\n";

use FindBin qw($Bin);

print "# $Bin\n";

if ($^O eq 'MacOS') {
    print "not " unless $Bin =~ m,:lib:$,;
} else {
    print "not " unless $Bin =~ m,[/.]lib\]?$,;
}
print "ok 1\n";

$0 = "-";
FindBin::again();

print "not " if $FindBin::Script ne "-";
print "ok 2\n";
