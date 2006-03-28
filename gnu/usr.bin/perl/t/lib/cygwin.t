#!perl

BEGIN {
    chdir 't' if -d 't';
    @INC = ('../lib');
    unless ($^O eq "cygwin") {
	print "1..0 # skipped: cygwin specific test\n";
	exit 0;
    }
}

use Test::More tests => 4;

is(Cygwin::winpid_to_pid(Cygwin::pid_to_winpid($$)), $$,
   "perl pid translates to itself");

my $parent = getppid;
SKIP: {
    skip "test not run from cygwin process", 1 if $parent <= 1;
    is(Cygwin::winpid_to_pid(Cygwin::pid_to_winpid($parent)), $parent,
       "parent pid translates to itself");
}

my $catpid = open my $cat, "|cat" or die "Couldn't cat: $!";
open my $ps, "ps|" or die "Couldn't do ps: $!";
my ($catwinpid) = map /^.\s+$catpid\s+\d+\s+\d+\s+(\d+)/, <$ps>;
close($ps);

is(Cygwin::winpid_to_pid($catwinpid), $catpid, "winpid to pid");
is(Cygwin::pid_to_winpid($catpid), $catwinpid, "pid to winpid");
close($cat);
