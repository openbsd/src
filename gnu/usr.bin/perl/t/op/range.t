#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}   

use Config;

print "1..45\n";

print join(':',1..5) eq '1:2:3:4:5' ? "ok 1\n" : "not ok 1\n";

@foo = (1,2,3,4,5,6,7,8,9);
@foo[2..4] = ('c','d','e');

print join(':',@foo[$foo[0]..5]) eq '2:c:d:e:6' ? "ok 2\n" : "not ok 2\n";

@bar[2..4] = ('c','d','e');
print join(':',@bar[1..5]) eq ':c:d:e:' ? "ok 3\n" : "not ok 3\n";

($a,@bcd[0..2],$e) = ('a','b','c','d','e');
print join(':',$a,@bcd[0..2],$e) eq 'a:b:c:d:e' ? "ok 4\n" : "not ok 4\n";

$x = 0;
for (1..100) {
    $x += $_;
}
print $x == 5050 ? "ok 5\n" : "not ok 5 $x\n";

$x = 0;
for ((100,2..99,1)) {
    $x += $_;
}
print $x == 5050 ? "ok 6\n" : "not ok 6 $x\n";

$x = join('','a'..'z');
print $x eq 'abcdefghijklmnopqrstuvwxyz' ? "ok 7\n" : "not ok 7 $x\n";

@x = 'A'..'ZZ';
print @x == 27 * 26 ? "ok 8\n" : "not ok 8\n";

@x = '09' .. '08';  # should produce '09', '10',... '99' (strange but true)
print "not " unless join(",", @x) eq
                    join(",", map {sprintf "%02d",$_} 9..99);
print "ok 9\n";

# same test with foreach (which is a separate implementation)
@y = ();
foreach ('09'..'08') {
    push(@y, $_);
}
print "not " unless join(",", @y) eq join(",", @x);
print "ok 10\n";

# check bounds
if ($Config{ivsize} == 8) {
  @a = eval "0x7ffffffffffffffe..0x7fffffffffffffff";
  $a = "9223372036854775806 9223372036854775807";
  @b = eval "-0x7fffffffffffffff..-0x7ffffffffffffffe";
  $b = "-9223372036854775807 -9223372036854775806";
}
else {
  @a = eval "0x7ffffffe..0x7fffffff";
  $a = "2147483646 2147483647";
  @b = eval "-0x7fffffff..-0x7ffffffe";
  $b = "-2147483647 -2147483646";
}

print "not " unless "@a" eq $a;
print "ok 11\n";

print "not " unless "@b" eq $b;
print "ok 12\n";

# check magic
{
    my $bad = 0;
    local $SIG{'__WARN__'} = sub { $bad = 1 };
    my $x = 'a-e';
    $x =~ s/(\w)-(\w)/join ':', $1 .. $2/e;
    $bad = 1 unless $x eq 'a:b:c:d:e';
    print $bad ? "not ok 13\n" : "ok 13\n";
}

# Should use magical autoinc only when both are strings
print "not " unless 0 == (() = "0"..-1);
print "ok 14\n";

for my $x ("0"..-1) {
    print "not ";
}
print "ok 15\n";

# [#18165] Should allow "-4".."0", broken by #4730. (AMS 20021031)
print join(":","-4".."0")      eq "-4:-3:-2:-1:0" ? "ok 16\n" : "not ok 16\n";
print join(":","-4".."-0")     eq "-4:-3:-2:-1:0" ? "ok 17\n" : "not ok 17\n";
print join(":","-4\n".."0\n")  eq "-4:-3:-2:-1:0" ? "ok 18\n" : "not ok 18\n";
print join(":","-4\n".."-0\n") eq "-4:-3:-2:-1:0" ? "ok 19\n" : "not ok 19\n";

# undef should be treated as 0 for numerical range
print join(":",undef..2) eq '0:1:2' ? "ok 20\n" : "not ok 20\n";
print join(":",-2..undef) eq '-2:-1:0' ? "ok 21\n" : "not ok 21\n";
print join(":",undef..'2') eq '0:1:2' ? "ok 22\n" : "not ok 22\n";
print join(":",'-2'..undef) eq '-2:-1:0' ? "ok 23\n" : "not ok 23\n";

# undef should be treated as "" for magical range
print join(":", map "[$_]", "".."B") eq '[]' ? "ok 24\n" : "not ok 24\n";
print join(":", map "[$_]", undef.."B") eq '[]' ? "ok 25\n" : "not ok 25\n";
print join(":", map "[$_]", "B".."") eq '' ? "ok 26\n" : "not ok 26\n";
print join(":", map "[$_]", "B"..undef) eq '' ? "ok 27\n" : "not ok 27\n";

# undef..undef used to segfault
print join(":", map "[$_]", undef..undef) eq '[]' ? "ok 28\n" : "not ok 28\n";

# also test undef in foreach loops
@foo=(); push @foo, $_ for undef..2;
print join(":", @foo) eq '0:1:2' ? "ok 29\n" : "not ok 29\n";

@foo=(); push @foo, $_ for -2..undef;
print join(":", @foo) eq '-2:-1:0' ? "ok 30\n" : "not ok 30\n";

@foo=(); push @foo, $_ for undef..'2';
print join(":", @foo) eq '0:1:2' ? "ok 31\n" : "not ok 31\n";

@foo=(); push @foo, $_ for '-2'..undef;
print join(":", @foo) eq '-2:-1:0' ? "ok 32\n" : "not ok 32\n";

@foo=(); push @foo, $_ for undef.."B";
print join(":", map "[$_]", @foo) eq '[]' ? "ok 33\n" : "not ok 33\n";

@foo=(); push @foo, $_ for "".."B";
print join(":", map "[$_]", @foo) eq '[]' ? "ok 34\n" : "not ok 34\n";

@foo=(); push @foo, $_ for "B"..undef;
print join(":", map "[$_]", @foo) eq '' ? "ok 35\n" : "not ok 35\n";

@foo=(); push @foo, $_ for "B".."";
print join(":", map "[$_]", @foo) eq '' ? "ok 36\n" : "not ok 36\n";

@foo=(); push @foo, $_ for undef..undef;
print join(":", map "[$_]", @foo) eq '[]' ? "ok 37\n" : "not ok 37\n";

# again with magic
{
    my @a = (1..3);
    @foo=(); push @foo, $_ for undef..$#a;
    print join(":", @foo) eq '0:1:2' ? "ok 38\n" : "not ok 38\n";
}
{
    my @a = ();
    @foo=(); push @foo, $_ for $#a..undef;
    print join(":", @foo) eq '-1:0' ? "ok 39\n" : "not ok 39\n";
}
{
    local $1;
    "2" =~ /(.+)/;
    @foo=(); push @foo, $_ for undef..$1;
    print join(":", @foo) eq '0:1:2' ? "ok 40\n" : "not ok 40\n";
}
{
    local $1;
    "-2" =~ /(.+)/;
    @foo=(); push @foo, $_ for $1..undef;
    print join(":", @foo) eq '-2:-1:0' ? "ok 41\n" : "not ok 41\n";
}
{
    local $1;
    "B" =~ /(.+)/;
    @foo=(); push @foo, $_ for undef..$1;
    print join(":", map "[$_]", @foo) eq '[]' ? "ok 42\n" : "not ok 42\n";
}
{
    local $1;
    "B" =~ /(.+)/;
    @foo=(); push @foo, $_ for ""..$1;
    print join(":", map "[$_]", @foo) eq '[]' ? "ok 43\n" : "not ok 43\n";
}
{
    local $1;
    "B" =~ /(.+)/;
    @foo=(); push @foo, $_ for $1..undef;
    print join(":", map "[$_]", @foo) eq '' ? "ok 44\n" : "not ok 44\n";
}
{
    local $1;
    "B" =~ /(.+)/;
    @foo=(); push @foo, $_ for $1.."";
    print join(":", map "[$_]", @foo) eq '' ? "ok 45\n" : "not ok 45\n";
}
