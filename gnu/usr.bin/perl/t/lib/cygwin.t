#!perl

BEGIN {
    chdir 't' if -d 't';
    @INC = ('../lib');
    unless ($^O eq "cygwin") {
	print "1..0 # skipped: cygwin specific test\n";
	exit 0;
    }
}

use Test::More tests => 16;

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

is(Cygwin::win_to_posix_path("t\\lib"), "t/lib", "win to posix path: t/lib");
is(Cygwin::posix_to_win_path("t/lib"), "t\\lib", "posix to win path: t\\lib");

use Win32;
use Cwd;
my $pwd = getcwd();
chdir("/");
my $winpath = Win32::GetCwd();
is(Cygwin::posix_to_win_path("/", 1), $winpath, "posix to absolute win path");
chdir($pwd);
is(Cygwin::win_to_posix_path($winpath, 1), "/", "win to absolute posix path");

my $mount = join '', `/usr/bin/mount`;
$mount =~ m|on /usr/bin type .+ \((\w+mode)[,\)]|m;
my $binmode = $1 eq 'binmode';
is(Cygwin::is_binmount("/"),  $binmode ? 1 : '', "check / for binmount");

my $rootmnt = Cygwin::mount_flags("/");
ok($binmode ? ($rootmnt =~ /,binmode/) : ($rootmnt =~ /,textmode/), "check / mount_flags");
is(Cygwin::mount_flags("/cygdrive") =~ /,cygdrive/,  1, "check cygdrive mount_flags");

# Cygdrive mount prefix
my @flags = split(/,/, Cygwin::mount_flags('/cygdrive'));
my $prefix = pop(@flags);
ok($prefix, "cygdrive mount prefix = " . (($prefix) ? $prefix : '<none>'));
chomp(my $prefix2 = `df | grep -i '^c: ' | cut -d% -f2 | xargs`);
$prefix2 =~ s/\/c$//i;
if (! $prefix2) {
    $prefix2 = '/';
}
is($prefix, $prefix2, 'cygdrive mount prefix');

my @mnttbl = Cygwin::mount_table();
ok(@mnttbl > 0, "non empty mount_table");
for $i (@mnttbl) {
  if ($i->[0] eq '/') {
    is($i->[2].",".$i->[3], $rootmnt, "same root mount flags");
    last;
  }
}

ok(Cwd->cwd(), "bug#38628 legacy");
