# test concurrent write and splice

use strict;
use warnings;
use POSIX;
use Time::HiRes;

our %args = (
    client => {
	len => 2**20,
    },
    relay => {
	func => sub {
	    defined(my $pid = fork())
		or die "relay func: fork failed: $!";
	    if ($pid == 0) {
		my $n;
		do {
		    $n = syswrite(STDOUT, "\n foo bar\n");
		    sleep .01;
		} while (!defined($n) || $n);
		POSIX::_exit(0);
	    }
	    # give the userland a moment to wite, even if splicing
	    sleep .1;
	    relay(@_);
	    kill 9, $pid;
	},
    },
    server => {
	func => sub { sleep 2; read_stream(@_); },
	# As syswrite() adds data to the socket, the content length is not
	# correct.  Disable the checks.
	nocheck => 1,
    },
    len => 1048576,
    md5 => '6649bbec13f3d7efaedf01c0cfa54f88',
);
