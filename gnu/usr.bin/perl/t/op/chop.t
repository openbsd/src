#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 139;

$_ = 'abc';
$c = foo();
is ($c . $_, 'cab', 'optimized');

$_ = 'abc';
$c = chop($_);
is ($c . $_ , 'cab', 'unoptimized');

sub foo {
    chop;
}

@foo = ("hi \n","there\n","!\n");
@bar = @foo;
chop(@bar);
is (join('',@bar), 'hi there!');

$foo = "\n";
chop($foo,@foo);
is (join('',$foo,@foo), 'hi there!');

$_ = "foo\n\n";
$got = chomp();
ok ($got == 1) or print "# got $got\n";
is ($_, "foo\n");

$_ = "foo\n";
$got = chomp();
ok ($got == 1) or print "# got $got\n";
is ($_, "foo");

$_ = "foo";
$got = chomp();
ok ($got == 0) or print "# got $got\n";
is ($_, "foo");

$_ = "foo";
$/ = "oo";
$got = chomp();
ok ($got == 2) or print "# got $got\n";
is ($_, "f");

$_ = "bar";
$/ = "oo";
$got = chomp();
ok ($got == 0) or print "# got $got\n";
is ($_, "bar");

$_ = "f\n\n\n\n\n";
$/ = "";
$got = chomp();
ok ($got == 5) or print "# got $got\n";
is ($_, "f");

$_ = "f\n\n";
$/ = "";
$got = chomp();
ok ($got == 2) or print "# got $got\n";
is ($_, "f");

$_ = "f\n";
$/ = "";
$got = chomp();
ok ($got == 1) or print "# got $got\n";
is ($_, "f");

$_ = "f";
$/ = "";
$got = chomp();
ok ($got == 0) or print "# got $got\n";
is ($_, "f");

$_ = "xx";
$/ = "xx";
$got = chomp();
ok ($got == 2) or print "# got $got\n";
is ($_, "");

$_ = "axx";
$/ = "xx";
$got = chomp();
ok ($got == 2) or print "# got $got\n";
is ($_, "a");

$_ = "axx";
$/ = "yy";
$got = chomp();
ok ($got == 0) or print "# got $got\n";
is ($_, "axx");

# This case once mistakenly behaved like paragraph mode.
$_ = "ab\n";
$/ = \3;
$got = chomp();
ok ($got == 0) or print "# got $got\n";
is ($_, "ab\n");

# Go Unicode.

$_ = "abc\x{1234}";
chop;
is ($_, "abc", "Go Unicode");

$_ = "abc\x{1234}d";
chop;
is ($_, "abc\x{1234}");

$_ = "\x{1234}\x{2345}";
chop;
is ($_, "\x{1234}");

my @stuff = qw(this that);
is (chop(@stuff[0,1]), 't');

# bug id 20010305.012
@stuff = qw(ab cd ef);
is (chop(@stuff = @stuff), 'f');

@stuff = qw(ab cd ef);
is (chop(@stuff[0, 2]), 'f');

my %stuff = (1..4);
is (chop(@stuff{1, 3}), '4');

# chomp should not stringify references unless it decides to modify them
$_ = [];
$/ = "\n";
$got = chomp();
ok ($got == 0) or print "# got $got\n";
is (ref($_), "ARRAY", "chomp ref (modify)");

$/ = ")";  # the last char of something like "ARRAY(0x80ff6e4)"
$got = chomp();
ok ($got == 1) or print "# got $got\n";
ok (!ref($_), "chomp ref (no modify)");

$/ = "\n";

%chomp = ("One" => "One", "Two\n" => "Two", "" => "");
%chop = ("One" => "On", "Two\n" => "Two", "" => "");

foreach (keys %chomp) {
  my $key = $_;
  eval {chomp $_};
  if ($@) {
    my $err = $@;
    $err =~ s/\n$//s;
    fail ("\$\@ = \"$err\"");
  } else {
    is ($_, $chomp{$key}, "chomp hash key");
  }
}

foreach (keys %chop) {
  my $key = $_;
  eval {chop $_};
  if ($@) {
    my $err = $@;
    $err =~ s/\n$//s;
    fail ("\$\@ = \"$err\"");
  } else {
    is ($_, $chop{$key}, "chop hash key");
  }
}

# chop and chomp can't be lvalues
eval 'chop($x) = 1;';
ok($@ =~ /Can\'t modify.*chop.*in.*assignment/);
eval 'chomp($x) = 1;';
ok($@ =~ /Can\'t modify.*chom?p.*in.*assignment/);
eval 'chop($x, $y) = (1, 2);';
ok($@ =~ /Can\'t modify.*chop.*in.*assignment/);
eval 'chomp($x, $y) = (1, 2);';
ok($@ =~ /Can\'t modify.*chom?p.*in.*assignment/);

my @chars = ("N", ord('A') == 193 ? "\xee" : "\xd3", substr ("\xd4\x{100}", 0, 1), chr 1296);
foreach my $start (@chars) {
  foreach my $end (@chars) {
    local $/ = $end;
    my $message = "start=" . ord ($start) . " end=" . ord $end;
    my $string = $start . $end;
    is (chomp ($string), 1, "$message [returns 1]");
    is ($string, $start, $message);

    my $end_utf8 = $end;
    utf8::encode ($end_utf8);
    next if $end_utf8 eq $end;

    # $end ne $end_utf8, so these should not chomp.
    $string = $start . $end_utf8;
    my $chomped = $string;
    is (chomp ($chomped), 0, "$message (end as bytes) [returns 0]");
    is ($chomped, $string, "$message (end as bytes)");

    $/ = $end_utf8;
    $string = $start . $end;
    $chomped = $string;
    is (chomp ($chomped), 0, "$message (\$/ as bytes) [returns 0]");
    is ($chomped, $string, "$message (\$/ as bytes)");
  }
}

{
    # returns length in characters, but not in bytes.
    $/ = "\x{100}";
    $a = "A$/";
    $b = chomp $a;
    is ($b, 1);

    $/ = "\x{100}\x{101}";
    $a = "A$/";
    $b = chomp $a;
    is ($b, 2);
}

{
    # [perl #36569] chop fails on decoded string with trailing nul
    my $asc = "perl\0";
    my $utf = "perl".pack('U',0); # marked as utf8
    is(chop($asc), "\0", "chopping ascii NUL");
    is(chop($utf), "\0", "chopping utf8 NUL");
    is($asc, "perl", "chopped ascii NUL");
    is($utf, "perl", "chopped utf8 NUL");
}

{
    # Change 26011: Re: A surprising segfault
    # to make sure only that these obfuscated sentences will not crash.

    map chop(+()), ('')x68;
    ok(1, "extend sp in pp_chop");

    map chomp(+()), ('')x68;
    ok(1, "extend sp in pp_chomp");
}
