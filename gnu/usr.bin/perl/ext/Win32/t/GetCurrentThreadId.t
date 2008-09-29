use strict;
use Config qw(%Config);
use Test;
use Win32;

unless ($Config{ccflags} =~ /PERL_IMPLICIT_SYS/) {
    print "1..0 # Skip: Test requires fork emulation\n";
    exit 0;
}

plan tests => 1;

if (my $pid = fork) {
    waitpid($pid, 0);
    exit 0;
}

# This test relies on the implementation detail that the fork() emulation
# uses the negative value of the thread id as a pseudo process id.
ok(-$$, Win32::GetCurrentThreadId());
