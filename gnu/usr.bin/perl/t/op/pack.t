#!./perl

# $RCSfile: pack.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:11 $

print "1..29\n";

$format = "c2 x5 C C x s d i l a6";
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

print +($x = unpack("I",pack("I", 0xFFFFFFFF))) == 0xFFFFFFFF
	? "ok 9\n" : "not ok 9 $x\n";

# check 'w'
my $test=10;
my @x = (5,130,256,560,32000,3097152,268435455,1073741844,
         '4503599627365785','23728385234614992549757750638446');
my $x = pack('w*', @x);
my $y = pack 'H*', '0581028200843081fa0081bd8440ffffff7f848080801487ffffffffffdb19caefe8e1eeeea0c2e1e3e8ede1ee6e';

print $x eq $y ? "ok $test\n" : "not ok $test\n"; $test++;

@y = unpack('w*', $y);
my $a;
while ($a = pop @x) {
  my $b = pop @y;
  print $a eq $b ? "ok $test\n" : "not ok $test\n$a\n$b\n"; $test++;
}

@y = unpack('w2', $x);

print scalar(@y) == 2 ? "ok $test\n" : "not ok $test\n"; $test++;
print $y[1] == 130 ? "ok $test\n" : "not ok $test\n"; $test++;

# test exeptions
eval { $x = unpack 'w', pack 'C*', 0xff, 0xff};
print $@ ne '' ? "ok $test\n" : "not ok $test\n"; $test++;

eval { $x = unpack 'w', pack 'C*', 0xff, 0xff, 0xff, 0xff};
print $@ ne '' ? "ok $test\n" : "not ok $test\n"; $test++;

eval { $x = unpack 'w', pack 'C*', 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
print $@ ne '' ? "ok $test\n" : "not ok $test\n"; $test++;

#
# test the "p" template

# literals
print((unpack("p",pack("p","foo")) eq "foo" ? "ok " : "not ok "),$test++,"\n");

# scalars
print((unpack("p",pack("p",$test)) == $test ? "ok " : "not ok "),$test++,"\n");

# temps
sub foo { my $a = "a"; return $a . $a++ . $a++ }
{
  local $^W = 1;
  my $last = $test;
  local $SIG{__WARN__} = sub {
	print "ok ",$test++,"\n" if $_[0] =~ /temporary val/
  };
  my $junk = pack("p", &foo);
  print "not ok ", $test++, "\n" if $last == $test;
}

# undef should give null pointer
print((pack("p", undef) =~ /^\0+/ ? "ok " : "not ok "),$test++,"\n");

