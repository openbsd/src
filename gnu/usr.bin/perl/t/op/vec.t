#!./perl

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc('../lib');
}

plan( tests => 35 );


is(vec($foo,0,1), 0);
is(length($foo), undef);
vec($foo,0,1) = 1;
is(length($foo), 1);
is(unpack('C',$foo), 1);
is(vec($foo,0,1), 1);

is(vec($foo,20,1), 0);
vec($foo,20,1) = 1;
is(vec($foo,20,1), 1);
is(length($foo), 3);
is(vec($foo,1,8), 0);
vec($foo,1,8) = 0xf1;
is(vec($foo,1,8), 0xf1);
is((unpack('C',substr($foo,1,1)) & 255), 0xf1);
is(vec($foo,2,4), 1);;
is(vec($foo,3,4), 15);
vec($Vec, 0, 32) = 0xbaddacab;
is($Vec, "\xba\xdd\xac\xab");
is(vec($Vec, 0, 32), 3135089835);

# ensure vec() handles numericalness correctly
$foo = $bar = $baz = 0;
vec($foo = 0,0,1) = 1;
vec($bar = 0,1,1) = 1;
$baz = $foo | $bar;
ok($foo eq "1" && $foo == 1);
ok($bar eq "2" && $bar == 2);
ok("$foo $bar $baz" eq "1 2 3");

# error cases

$x = eval { vec $foo, 0, 3 };
like($@, qr/^Illegal number of bits in vec/);
$@ = undef;
$x = eval { vec $foo, 0, 0 };
like($@, qr/^Illegal number of bits in vec/);
$@ = undef;
$x = eval { vec $foo, 0, -13 };
like($@, qr/^Illegal number of bits in vec/);
$@ = undef;
$x = eval { vec($foo, -1, 4) = 2 };
like($@, qr/^Negative offset to vec in lvalue context/);
$@ = undef;
ok(! vec('abcd', 7, 8));

# UTF8
# N.B. currently curiously coded to circumvent bugs elswhere in UTF8 handling

$foo = "\x{100}" . "\xff\xfe";
$x = substr $foo, 1;
is(vec($x, 0, 8), 255);
$@ = undef;
eval { vec($foo, 1, 8) };
ok(! $@);
$@ = undef;
eval { vec($foo, 1, 8) = 13 };
ok(! $@);
if ($::IS_EBCDIC) {
    is($foo, "\x8c\x0d\xff\x8a\x69"); 
}
else {
    is($foo, "\xc4\x0d\xc3\xbf\xc3\xbe");
}
$foo = "\x{100}" . "\xff\xfe";
$x = substr $foo, 1;
vec($x, 2, 4) = 7;
is($x, "\xff\xf7");

# mixed magic

$foo = "\x61\x62\x63\x64\x65\x66";
is(vec(substr($foo, 2, 2), 0, 16), 25444);
vec(substr($foo, 1,3), 5, 4) = 3;
is($foo, "\x61\x62\x63\x34\x65\x66");

# A variation of [perl #20933]
{
    my $s = "";
    vec($s, 0, 1) = 0;
    vec($s, 1, 1) = 1;
    my @r;
    $r[$_] = \ vec $s, $_, 1 for (0, 1);
    ok(!(${ $r[0] } != 0 || ${ $r[1] } != 1)); 
}


my $destroyed;
{ package Class; DESTROY { ++$destroyed; } }

$destroyed = 0;
{
    my $x = '';
    vec($x,0,1) = 0;
    $x = bless({}, 'Class');
}
is($destroyed, 1, 'Timely scalar destruction with lvalue vec');

use constant roref => \1;
eval { for (roref) { vec($_,0,1) = 1 } };
like($@, qr/^Modification of a read-only value attempted at /,
        'err msg when modifying read-only refs');


{
    # downgradeable utf8 strings should be downgraded before accessing
    # the byte string.
    # See the p5p thread with Message-ID:
    # <CAMx+QJ6SAv05nmpnc7bmp0Wo+sjcx=ssxCcE-P_PZ8HDuCQd9A@mail.gmail.com>


    my $x = substr "\x{100}\xff\xfe", 1; # a utf8 string with all ords < 256
    my $v;
    $v = vec($x, 0, 8);
    is($v, 255, "downgraded utf8 try 1");
    $v = vec($x, 0, 8);
    is($v, 255, "downgraded utf8 try 2");
}
