#!./perl

# $RCSfile: pack.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:11 $

print "1..8\n";

$format = "c2x5CCxsdila6";
# Need the expression in here to force ary[5] to be numeric.  This avoids
# test2 failing because ary2 goes str->numeric->str and ary doesn't.
@ary = (1,-100,127,128,32767,987.654321098 / 100.0,12345,123456,"abcdef");
$foo = pack($format,@ary);
@ary2 = unpack($format,$foo);

print ($#ary == $#ary2 ? "ok 1\n" : "not ok 1\n");

$out1=join(':',@ary);
$out2=join(':',@ary2);
print ($out1 eq $out2 ? "ok 2\n" : "not ok 2\n");

print ($foo =~ /def/ ? "ok 3\n" : "not ok 3\n");

# How about counting bits?

print +($x = unpack("%32B*", "\001\002\004\010\020\040\100\200\377")) == 16
	? "ok 4\n" : "not ok 4 $x\n";

print +($x = unpack("%32b69", "\001\002\004\010\020\040\100\200\017")) == 12
	? "ok 5\n" : "not ok 5 $x\n";

print +($x = unpack("%32B69", "\001\002\004\010\020\040\100\200\017")) == 9
	? "ok 6\n" : "not ok 6 $x\n";

print +($x = unpack("%32B*", "Now is the time for all good blurfl")) == 129
	? "ok 7\n" : "not ok 7 $x\n";

open(BIN, "./perl") || open(BIN, "./perl.exe") 
    || die "Can't open ../perl or ../perl.exe: $!\n";
sysread BIN, $foo, 8192;
close BIN;

$sum = unpack("%32b*", $foo);
$longway = unpack("b*", $foo);
print $sum == $longway =~ tr/1/1/ ? "ok 8\n" : "not ok 8\n";
