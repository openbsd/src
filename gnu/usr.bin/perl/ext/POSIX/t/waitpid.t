BEGIN {
	chdir 't' if -d 't';
	unshift @INC, '../lib';
}

BEGIN {
    use Config;
    unless ($Config{d_fork}) {
	print "1..0 # Skip: no fork\n";
	exit 0;
    }
    eval 'use POSIX qw(sys_wait_h)';
    if ($@) {
	print "1..0 # Skip: no POSIX sys_wait_h\n";
	exit 0;
    }
    eval 'use Time::HiRes qw(time)';
    if ($@) {
	print "1..0 # Skip: no Time::HiRes\n";
	exit 0;
    }
}

use warnings;
use strict;

$| = 1;

print "1..1\n";

sub NEG1_PROHIBITED () { 0x01 }
sub NEG1_REQUIRED   () { 0x02 }

my $count     = 0;
my $max_count = 9;
my $state     = NEG1_PROHIBITED;

my $child_pid = fork();

# Parent receives a nonzero child PID.

if ($child_pid) {
    my $ok = 1;

    while ($count++ < $max_count) {   
	my $begin_time = time();        
	my $ret = waitpid( -1, WNOHANG );          
	my $elapsed_time = time() - $begin_time;
	
	printf( "# waitpid(-1,WNOHANG) returned %d after %.2f seconds\n",
		$ret, $elapsed_time );
	if ($elapsed_time > 0.5) {
	    printf( "# %.2f seconds in non-blocking waitpid is too long!\n",
		    $elapsed_time );
	    $ok = 0;
	    last;
	}
	
	if ($state & NEG1_PROHIBITED) { 
	    if ($ret == -1) {
		print "# waitpid should not have returned -1 here!\n";
		$ok = 0;
		last;
	    }
	    elsif ($ret == $child_pid) {
		$state = NEG1_REQUIRED;
	    }
	}
	elsif ($state & NEG1_REQUIRED) {
	    unless ($ret == -1) {
		print "# waitpid should have returned -1 here\n";
		$ok = 0;
	    }
	    last;
	}
	
	sleep(1);
    }
    print $ok ? "ok 1\n" : "not ok 1\n";
    exit(0); # parent 
} else {
    # Child receives a zero PID and can request parent's PID with
    # getppid().
    sleep(3);
    exit(0);
}


