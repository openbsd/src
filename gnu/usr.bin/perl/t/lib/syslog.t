#!./perl

BEGIN {
    chdir 't' if -d 't';
    unshift @INC, '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bSyslog\b/) {
	print "1..0 # Skip: Sys::Syslog was not built\n";
	exit 0;
    }
}

use Sys::Syslog qw(:DEFAULT setlogsock);

print "1..6\n";

print defined(eval { setlogsock('unix') }) ? "ok 1\n" : "not ok 1\n";
print defined(eval { openlog('perl', 'ndelay', 'local0') }) ? "ok 2\n" : "not ok 2\n";
print defined(eval { syslog('info', 'test') }) ? "ok 3\n" : "not ok 3\n";

print defined(eval { setlogsock('inet') }) ? "ok 4\n" : "not ok 4\n";
print defined(eval { openlog('perl', 'ndelay', 'local0') }) ? "ok 5\n" : "not ok 5\n";
print defined(eval { syslog('info', 'test') }) ? "ok 6\n" : "not ok 6\n";
