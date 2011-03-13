# test server reads and exits after reading some data

use strict;
use warnings;

our %args = (
    client => {
	func => sub { errignore(@_); write_char(@_); },
	len => 2**30,  # not reached
	sndbuf => 2**10,  # small buffer triggers error during write
	# the error message seems to be timing dependent
	down => "Client print failed: (Broken pipe|Connection reset by peer)",
	nocheck => 1,
	error => 54,
    },
    relay => {
	func => sub { errignore(@_); relay(@_); },
	rcvbuf => 2**10,
	sndbuf => 2**10,
	down => "Broken pipe|Connection reset by peer",
	nocheck => 1,
	errorin => 0,  # syscall has read the error and resetted it
	errorout => 54,
    },
    server => {
	func => sub { $SIG{ALRM} = sub { print STDERR "\nShutdown\n"; exit 0 };
	  alarm(3); read_char(@_); },
	nocheck => 1,
    },
    noecho => 1,
);

1;
