#!./perl

# Test that getppid() follows UNIX semantics: when the parent process
# dies, the child is reparented to the init process (pid 1).

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(../lib);
}

use strict;
use Config;

BEGIN {
    for my $syscall (qw(pipe fork waitpid getppid)) {
	if (!$Config{"d_$syscall"}) {
	    print "1..0 # Skip: no $syscall\n";
	    exit;
	}
    }
    print "1..3\n";
}

pipe my ($r, $w) or die "pipe: $!\n";
my $pid = fork; defined $pid or die "fork: $!\n";

if ($pid) {
    # parent
    close $w;
    waitpid($pid, 0) == $pid or die "waitpid: $!\n";
    print <$r>;
}
else {
    # child
    close $r;
    my $pid2 = fork; defined $pid2 or die "fork: $!\n";
    if ($pid2) {
	close $w;
	sleep 1;
    }
    else {
	# grandchild
	my $ppid1 = getppid();
	print $w "not " if $ppid1 <= 1;
	print $w "ok 1 # ppid1=$ppid1\n";
	sleep 2;
	my $ppid2 = getppid();
	print $w "not " if $ppid1 == $ppid2;
	print $w "ok 2 # ppid2=$ppid2, ppid1!=ppid2\n";
	print $w "not " if $ppid2 != 1;
	print $w "ok 3 # ppid2=1\n";
    }
    exit 0;
}
