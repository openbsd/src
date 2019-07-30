#!./perl

# Minimally test if dump() behaves as expected

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
    require './test.pl';

    skip_all_if_miniperl();
}

use Config;
use File::Temp qw(tempdir);
use Cwd qw(getcwd);
use File::Spec;

skip_all("only tested on devel builds")
  unless $Config{usedevel};

# there may be other operating systems where it makes sense, but
# there are some where it isn't, so limit the platforms we test
# this on. Also this needs to be a platform that fully supports
# fork() and waitpid().

skip_all("no point in dumping on $^O")
  unless $^O =~ /^(linux|.*bsd|solaris)$/;

skip_all("avoid coredump under ASan")
  if  $Config{ccflags} =~ /-fsanitize=/;

# execute in a work directory so File::Temp can clean up core dumps
my $tmp = tempdir(CLEANUP => 1);

my $start = getcwd;

# on systems which don't make $^X absolute which_perl() in test.pl won't
# return an absolute path, so once we change directories it can't
# find ./perl, resulting in test failures
$^X = File::Spec->rel2abs($^X);

chdir $tmp
  or skip_all("Cannot chdir to work directory");

plan(2);

# Depending on how perl is built, there may be extraneous stuff on stderr
# such as "Aborted", which isn't caught by the '2>&1' that
# fresh_perl_like() does. So execute each dump() in a sub-process.
#
# In detail:
# fresh_perl_like() ends up doing a `` which invokes a shell with 2 args:
#
#   "sh", "-c", "perl /tmp/foo 2>&1"
#
# When the perl process coredumps after calling dump(), the parent
# sh sees that the exit of the child flags a coredump and so prints
# something like the following to stderr:
#
#    sh: line 1: 17605 Aborted (core dumped)
#
# Note that the '2>&1' only applies to the perl process, not to the sh
# command itself.
# By do the dump in a child, the parent perl process exits back to sh with
# a normal exit value, so sh won't complain.

fresh_perl_like(<<'PROG', qr/\AA(?!B\z)/, {}, "plain dump quits");
++$|;
my $pid = fork;
die "fork: $!\n" unless defined $pid;
if ($pid) {
    # parent
    waitpid($pid, 0);
}
else {
    # child
    print qq(A);
    dump;
    print qq(B);
}
PROG

fresh_perl_like(<<'PROG', qr/A(?!B\z)/, {}, "dump with label quits");
++$|;
my $pid = fork;
die "fork: $!\n" unless defined $pid;
if ($pid) {
    # parent
    waitpid($pid, 0);
}
else {
    print qq(A);
    dump foo;
    foo:
    print qq(B);
}
PROG

END {
  chdir $start if defined $start;
}
