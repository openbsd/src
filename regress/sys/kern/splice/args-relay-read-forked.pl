# test concurrent read and splice

use strict;
use warnings;

our %args = (
    client => {
	len => 2**20,
    },
    relay => {
	nonblocking => 1,
	func => sub {
	    defined(my $pid = fork())
		or die "relay func: fork failed: $!";
	    if ($pid) {
		relay(@_);
		return;
	    }
	    my $n;
	    do {
		$n = sysread(STDIN, my $buf, 10);
	    } while (!defined($n) || $n);
	    POSIX::_exit(0);
	},
    },
    server => {
	func => sub { sleep 2; read_char(@_); },
    },
    # As sysread() may extract data from the socket before splicing starts,
    # the spliced content length is not reliable.  Disable the checks.
    nocheck => 1,
    len => 1048576,
    md5 => '6649bbec13f3d7efaedf01c0cfa54f88',
);

1;
