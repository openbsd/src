BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..6\n";

my $a = chr(0x0100);

print ord($a)     == 0x100 ? "ok 1\n" : "not ok 1\n";
print length($a)  ==     1 ? "ok 2\n" : "not ok 2\n";

{
    use bytes;
    my $b = chr(0x0100);
    print ord($b) ==     0 ? "ok 3\n" : "not ok 3\n";
}

my $c = chr(0x0100);

print ord($c)     == 0x100 ? "ok 4\n" : "not ok 4\n";

{
    use bytes;
    if (ord('A') == 193) {
	print ord($c) == 0x8c ? "ok 5\n" : "not ok 5\n";
    } else {
	print ord($c) == 0xc4 ? "ok 5\n" : "not ok 5\n";
    }
    print length($c) == 2 ? "ok 6\n" : "not ok 6\n";
}

