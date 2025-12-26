#!./perl

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc('../lib');
}

use strict;
use warnings;
our ($fh, @fh, %fh);

eval 'opendir(NOSUCH, "no/such/directory");';
skip_all($@) if $@;

for my $i (1..2000) {
    local *OP;
    opendir(OP, "op") or die "can't opendir: $!";
    # should auto-closedir() here
}

is(opendir(OP, "op"), 1);
my @D = grep(/^[^\.].*\.t$/i, readdir(OP));
closedir(OP);

my $expect;
{
    open my $man, '<', '../MANIFEST' or die "Can't open ../MANIFEST: $!";
    while (<$man>) {
	++$expect if m!^t/op/[^/]+\t!;
    }
}

my ($min, $max) = ($expect - 10, $expect + 10);
within(scalar @D, $expect, 10, 'counting op/*.t');

my @R = sort @D;
my @G = sort <op/*.t>;
if ($G[0] =~ m#.*\](\w+\.t)#i) {
    # grep is to convert filespecs returned from glob under VMS to format
    # identical to that returned by readdir
    @G = grep(s#.*\](\w+\.t).*#op/$1#i,<op/*.t>);
}
while (@R && @G && $G[0] eq 'op/'.$R[0]) {
	shift(@R);
	shift(@G);
}
is(scalar @R, 0, 'readdir results all accounted for');
is(scalar @G, 0, 'glob results all accounted for');

is(opendir($fh, "op"), 1);
is(ref $fh, 'GLOB');
is(opendir($fh[0], "op"), 1);
is(ref $fh[0], 'GLOB');
is(opendir($fh{abc}, "op"), 1);
is(ref $fh{abc}, 'GLOB');
isnt("$fh", "$fh[0]");
isnt("$fh", "$fh{abc}");

# See that perl does not segfault upon readdir($x=".");
# https://github.com/Perl/perl5/issues/9813
fresh_perl_like(<<'EOP', qr/^no crash/, {}, 'GH #9813');
  eval {
    my $x = ".";
    my @files = readdir($x);
  };
  print "no crash";
EOP

SKIP:
{ # [perl #118651]
  # test that readdir doesn't modify errno on successfully reaching the end of the list
  # in scalar context, POSIX requires that readdir() not modify errno on end-of-directory

  my @s;
  ok(opendir(OP, "op"), "opendir op");
  $! = 0;
  while (defined(my $f = readdir OP)) {
    push @s, $f
      if $f =~ /^[^\.].*\.t$/i;
  }
  my $errno = $! + 0;
  closedir OP;
  is(@s, @D, "should be the same number of files, scalar or list")
    or skip "mismatch on file count - presumably a readdir error", 1;
  is($errno, 0, "errno preserved");
}

SKIP:
{
    open my $fh, "<", "op"
      or skip "can't open a directory on this platform", 10;
    my $warned;
    local $SIG{__WARN__} = sub { $warned = "@_" };
    ok(!readdir($fh), "cannot readdir file handle");
    like($warned, qr/readdir\(\) attempted on handle \$fh opened with open/,
         "check the message");
    undef $warned;
    ok(!telldir($fh), "cannot telldir file handle");
    like($warned, qr/telldir\(\) attempted on handle \$fh opened with open/,
         "check the message");
    undef $warned;
    ok(!seekdir($fh, 0), "cannot seekdir file handle");
    like($warned, qr/seekdir\(\) attempted on handle \$fh opened with open/,
         "check the message");
    undef $warned;
    ok(!rewinddir($fh), "cannot rewinddir file handle");
    like($warned, qr/rewinddir\(\) attempted on handle \$fh opened with open/,
         "check the message");
    undef $warned;
    ok(!closedir($fh), "cannot closedir file handle");
    like($warned, qr/closedir\(\) attempted on handle \$fh opened with open/,
         "check the message");
    undef $warned;
}

done_testing();
