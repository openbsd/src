#!./perl

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    @INC = '../lib';
}

plan (tests => 34);

print "not " unless length("")    == 0;
print "ok 1\n";

print "not " unless length("abc") == 3;
print "ok 2\n";

$_ = "foobar";
print "not " unless length()      == 6;
print "ok 3\n";

# Okay, so that wasn't very challenging.  Let's go Unicode.

{
    my $a = "\x{41}";

    print "not " unless length($a) == 1;
    print "ok 4\n";
    $test++;

    use bytes;
    print "not " unless $a eq "\x41" && length($a) == 1;
    print "ok 5\n";
    $test++;
}

{
    my $a = pack("U", 0xFF);

    print "not " unless length($a) == 1;
    print "ok 6\n";
    $test++;

    use bytes;
    if (ord('A') == 193)
     {
      printf "#%vx for 0xFF\n",$a;
      print "not " unless $a eq "\x8b\x73" && length($a) == 2;
     }
    else
     {
      print "not " unless $a eq "\xc3\xbf" && length($a) == 2;
     }
    print "ok 7\n";
    $test++;
}

{
    my $a = "\x{100}";

    print "not " unless length($a) == 1;
    print "ok 8\n";
    $test++;

    use bytes;
    if (ord('A') == 193)
     {
      printf "#%vx for 0x100\n",$a;
      print "not " unless $a eq "\x8c\x41" && length($a) == 2;
     }
    else
     {
      print "not " unless $a eq "\xc4\x80" && length($a) == 2;
     }
    print "ok 9\n";
    $test++;
}

{
    my $a = "\x{100}\x{80}";

    print "not " unless length($a) == 2;
    print "ok 10\n";
    $test++;

    use bytes;
    if (ord('A') == 193)
     {
      printf "#%vx for 0x100 0x80\n",$a;
      print "not " unless $a eq "\x8c\x41\x8a\x67" && length($a) == 4;
     }
    else
     {
      print "not " unless $a eq "\xc4\x80\xc2\x80" && length($a) == 4;
     }
    print "ok 11\n";
    $test++;
}

{
    my $a = "\x{80}\x{100}";

    print "not " unless length($a) == 2;
    print "ok 12\n";
    $test++;

    use bytes;
    if (ord('A') == 193)
     {
      printf "#%vx for 0x80 0x100\n",$a;
      print "not " unless $a eq "\x8a\x67\x8c\x41" && length($a) == 4;
     }
    else
     {
      print "not " unless $a eq "\xc2\x80\xc4\x80" && length($a) == 4;
     }
    print "ok 13\n";
    $test++;
}

# Now for Unicode with magical vtbls

{
    require Tie::Scalar;
    my $a;
    tie $a, 'Tie::StdScalar';  # makes $a magical
    $a = "\x{263A}";
    
    print "not " unless length($a) == 1;
    print "ok 14\n";
    $test++;

    use bytes;
    print "not " unless length($a) == 3;
    print "ok 15\n";
    $test++;
}

{
    # Play around with Unicode strings,
    # give a little workout to the UTF-8 length cache.
    my $a = chr(256) x 100;
    print length $a == 100 ? "ok 16\n" : "not ok 16\n";
    chop $a;
    print length $a ==  99 ? "ok 17\n" : "not ok 17\n";
    $a .= $a;
    print length $a == 198 ? "ok 18\n" : "not ok 18\n";
    $a = chr(256) x 999;
    print length $a == 999 ? "ok 19\n" : "not ok 19\n";
    substr($a, 0, 1) = '';
    print length $a == 998 ? "ok 20\n" : "not ok 20\n";
}

curr_test(21);

require Tie::Scalar;

$u = "ASCII";

tie $u, 'Tie::StdScalar', chr 256;

is(length $u, 1, "Length of a UTF-8 scalar returned from tie");
is(length $u, 1, "Again! Again!");

$^W = 1;

my $warnings = 0;

$SIG{__WARN__} = sub {
    $warnings++;
    warn @_;
};

is(length(undef), undef, "Length of literal undef");

my $u;

is(length($u), undef, "Length of regular scalar");

$u = "Gotcha!";

tie $u, 'Tie::StdScalar';

is(length($u), undef, "Length of tied scalar (MAGIC)");

is($u, undef);

{
    package U;
    use overload '""' => sub {return undef;};
}

my $uo = bless [], 'U';

is(length($uo), undef, "Length of overloaded reference");

my $ul = 3;
is(($ul = length(undef)), undef, 
                    "Returned length of undef with result in TARG");
is($ul, undef, "Assigned length of undef with result in TARG");

$ul = 3;
is(($ul = length($u)), undef,
                "Returned length of tied undef with result in TARG");
is($ul, undef, "Assigned length of tied undef with result in TARG");

$ul = 3;
is(($ul = length($uo)), undef,
                "Returned length of overloaded undef with result in TARG");
is($ul, undef, "Assigned length of overloaded undef with result in TARG");

# ok(!defined $uo); Turns you can't test this. FIXME for pp_defined?

is($warnings, 0, "There were no warnings");
