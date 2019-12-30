#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    require "./test.pl";
    set_up_inc('../lib');
}

use Config;

$Config{d_getcwd}
  or plan skip_all => "no getcwd";

my $cwd = Internals::getcwd();
ok(!defined $cwd || $cwd ne "",
   "Internals::getcwd() returned a reasonable result");

if (defined $cwd) {
    ok(-d $cwd, "check a success result is a directory");
}

done_testing();
