#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 47;

$_ = 'abc';
$c = do foo();
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
