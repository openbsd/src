#!./perl

# Test that getppid() follows UNIX semantics: when the parent process
# dies, the child is reparented to the init process
# The init process is usually 1, but doesn't have to be, and there's no
# standard way to find out what it is, so the only portable way to go it so
# attempt 2 reparentings and see if the PID both orphaned grandchildren get is
# the same. (and not ours)

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
    require './test.pl';
    plan (8);
}

sub fork_and_retrieve {
    my $which = shift;
    pipe my ($r, $w) or die "pipe: $!\n";
    my $pid = fork; defined $pid or die "fork: $!\n";

    if ($pid) {
	# parent
	close $w;
	$_ = <$r>;
	chomp;
	die "Garbled output '$_'"
	    unless my ($first, $second) = /^(\d+),(\d+)\z/;
	cmp_ok ($first, '>=', 1, "Parent of $which grandchild");
	cmp_ok ($second, '>=', 1, "New parent of orphaned $which grandchild");
	SKIP: {
	    skip("Orphan processes are not reparented on QNX", 1)
		if $^O eq 'nto';
	    isnt($first, $second,
                 "Orphaned $which grandchild got a new parent");
	}
	return $second;
    }
    else {
	# child
	# Prevent test.pl from thinking that we failed to run any tests.
	$::NO_ENDING = 1;
	close $r;

	my $pid2 = fork; defined $pid2 or die "fork: $!\n";
	if ($pid2) {
	    close $w;
	    sleep 1;
	}
	else {
	    # grandchild
	    my $ppid1 = getppid();
	    # Wait for immediate parent to exit
	    sleep 2;
	    my $ppid2 = getppid();
	    print $w "$ppid1,$ppid2\n";
	}
	exit 0;
    }
}

my $first = fork_and_retrieve("first");
my $second = fork_and_retrieve("second");
SKIP: {
    skip ("Orphan processes are not reparented on QNX", 1) if $^O eq 'nto';
    is ($first, $second, "Both orphaned grandchildren get the same new parent");
}
isnt ($first, $$, "And that new parent isn't this process");
