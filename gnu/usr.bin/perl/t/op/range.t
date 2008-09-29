#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = ('../lib', '.');
}   
# Avoid using eq_array below as it uses .. internally.
require 'test.pl';

use Config;

plan (45);

is(join(':',1..5), '1:2:3:4:5');

@foo = (1,2,3,4,5,6,7,8,9);
@foo[2..4] = ('c','d','e');

is(join(':',@foo[$foo[0]..5]), '2:c:d:e:6');

@bar[2..4] = ('c','d','e');
is(join(':',@bar[1..5]), ':c:d:e:');

($a,@bcd[0..2],$e) = ('a','b','c','d','e');
is(join(':',$a,@bcd[0..2],$e), 'a:b:c:d:e');

$x = 0;
for (1..100) {
    $x += $_;
}
is($x, 5050);

$x = 0;
for ((100,2..99,1)) {
    $x += $_;
}
is($x, 5050);

$x = join('','a'..'z');
is($x, 'abcdefghijklmnopqrstuvwxyz');

@x = 'A'..'ZZ';
is (scalar @x, 27 * 26);

@x = '09' .. '08';  # should produce '09', '10',... '99' (strange but true)
is(join(",", @x), join(",", map {sprintf "%02d",$_} 9..99));

# same test with foreach (which is a separate implementation)
@y = ();
foreach ('09'..'08') {
    push(@y, $_);
}
is(join(",", @y), join(",", @x));

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

is ("@a", $a);

is ("@b", $b);

# check magic
{
    my $bad = 0;
    local $SIG{'__WARN__'} = sub { $bad = 1 };
    my $x = 'a-e';
    $x =~ s/(\w)-(\w)/join ':', $1 .. $2/e;
    is ($x, 'a:b:c:d:e');
}

# Should use magical autoinc only when both are strings
{
    my $scalar = (() = "0"..-1);
    is ($scalar, 0);
}
{
    my $fail = 0;
    for my $x ("0"..-1) {
	$fail++;
    }
    is ($fail, 0);
}

# [#18165] Should allow "-4".."0", broken by #4730. (AMS 20021031)
is(join(":","-4".."0")     , "-4:-3:-2:-1:0");
is(join(":","-4".."-0")    , "-4:-3:-2:-1:0");
is(join(":","-4\n".."0\n") , "-4:-3:-2:-1:0");
is(join(":","-4\n".."-0\n"), "-4:-3:-2:-1:0");

# undef should be treated as 0 for numerical range
is(join(":",undef..2), '0:1:2');
is(join(":",-2..undef), '-2:-1:0');
is(join(":",undef..'2'), '0:1:2');
is(join(":",'-2'..undef), '-2:-1:0');

# undef should be treated as "" for magical range
is(join(":", map "[$_]", "".."B"), '[]');
is(join(":", map "[$_]", undef.."B"), '[]');
is(join(":", map "[$_]", "B"..""), '');
is(join(":", map "[$_]", "B"..undef), '');

# undef..undef used to segfault
is(join(":", map "[$_]", undef..undef), '[]');

# also test undef in foreach loops
@foo=(); push @foo, $_ for undef..2;
is(join(":", @foo), '0:1:2');

@foo=(); push @foo, $_ for -2..undef;
is(join(":", @foo), '-2:-1:0');

@foo=(); push @foo, $_ for undef..'2';
is(join(":", @foo), '0:1:2');

@foo=(); push @foo, $_ for '-2'..undef;
is(join(":", @foo), '-2:-1:0');

@foo=(); push @foo, $_ for undef.."B";
is(join(":", map "[$_]", @foo), '[]');

@foo=(); push @foo, $_ for "".."B";
is(join(":", map "[$_]", @foo), '[]');

@foo=(); push @foo, $_ for "B"..undef;
is(join(":", map "[$_]", @foo), '');

@foo=(); push @foo, $_ for "B".."";
is(join(":", map "[$_]", @foo), '');

@foo=(); push @foo, $_ for undef..undef;
is(join(":", map "[$_]", @foo), '[]');

# again with magic
{
    my @a = (1..3);
    @foo=(); push @foo, $_ for undef..$#a;
    is(join(":", @foo), '0:1:2');
}
{
    my @a = ();
    @foo=(); push @foo, $_ for $#a..undef;
    is(join(":", @foo), '-1:0');
}
{
    local $1;
    "2" =~ /(.+)/;
    @foo=(); push @foo, $_ for undef..$1;
    is(join(":", @foo), '0:1:2');
}
{
    local $1;
    "-2" =~ /(.+)/;
    @foo=(); push @foo, $_ for $1..undef;
    is(join(":", @foo), '-2:-1:0');
}
{
    local $1;
    "B" =~ /(.+)/;
    @foo=(); push @foo, $_ for undef..$1;
    is(join(":", map "[$_]", @foo), '[]');
}
{
    local $1;
    "B" =~ /(.+)/;
    @foo=(); push @foo, $_ for ""..$1;
    is(join(":", map "[$_]", @foo), '[]');
}
{
    local $1;
    "B" =~ /(.+)/;
    @foo=(); push @foo, $_ for $1..undef;
    is(join(":", map "[$_]", @foo), '');
}
{
    local $1;
    "B" =~ /(.+)/;
    @foo=(); push @foo, $_ for $1.."";
    is(join(":", map "[$_]", @foo), '');
}
