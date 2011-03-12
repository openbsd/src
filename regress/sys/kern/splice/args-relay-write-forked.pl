# test concurrent write and splice

use strict;
use warnings;
use Time::HiRes 'sleep';

our %args = (
    client => {
	len => 2**20,
    },
    relay => {
	func => sub {
	    defined(my $pid = fork())
		or die "relay func: fork failed: $!";
	    if ($pid) {
		relay(@_);
		kill 15, $pid;
		return;
	    }
	    my $n;
	    do {
		$n = syswrite(STDOUT, "\n foo bar\n");
		sleep .1;
	    } while (defined($n));
	    POSIX::_exit(0);
	},
    },
    server => {
	func => sub { sleep 2; read_char(@_); },
    },
    # As syswrite() adds data to the socket, the content length is not
    # correct.  Disable the checks.
    nocheck => 1,
    len => 1048576,
    md5 => '6649bbec13f3d7efaedf01c0cfa54f88',
);

1;
