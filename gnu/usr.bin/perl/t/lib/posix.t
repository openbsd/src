#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($^O ne 'VMS' and $Config{'extensions'} !~ /\bPOSIX\b/) {
	print "1..0\n";
	exit 0;
    }
}

use POSIX qw(fcntl_h signal_h limits_h _exit getcwd open read write);
use strict subs;

$| = 1;
print "1..14\n";

$testfd = open("TEST", O_RDONLY, 0) and print "ok 1\n";
read($testfd, $buffer, 9) if $testfd > 2;
print $buffer eq "#!./perl\n" ? "ok 2\n" : "not ok 2\n";

write(1,"ok 3\nnot ok 3\n", 5);

@fds = POSIX::pipe();
print $fds[0] > $testfd ? "ok 4\n" : "not ok 4\n";
CORE::open($reader = \*READER, "<&=".$fds[0]);
CORE::open($writer = \*WRITER, ">&=".$fds[1]);
print $writer "ok 5\n";
close $writer;
print <$reader>;
close $reader;

$sigset = new POSIX::SigSet 1,3;
delset $sigset 1;
if (!ismember $sigset 1) { print "ok 6\n" }
if (ismember $sigset 3) { print "ok 7\n" }
$mask = new POSIX::SigSet &SIGINT;
$action = new POSIX::SigAction 'main::SigHUP', $mask, 0;
sigaction(&SIGHUP, $action);
$SIG{'INT'} = 'SigINT';
kill 'HUP', $$;
sleep 1;
print "ok 11\n";

sub SigHUP {
    print "ok 8\n";
    kill 'INT', $$;
    sleep 2;
    print "ok 9\n";
}

sub SigINT {
    print "ok 10\n";
}

print &_POSIX_OPEN_MAX > $fds[1] ? "ok 12\n" : "not ok 12\n";

print getcwd() =~ m#/t$# ? "ok 13\n" : "not ok 13\n";

# Pick up whether we're really able to dynamically load everything.
print &POSIX::acos(1.0) == 0.0 ? "ok 14\n" : "not ok 14\n";

$| = 0;
print '@#!*$@(!@#$';
_exit(0);
