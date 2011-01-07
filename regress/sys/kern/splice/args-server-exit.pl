# test server sleeps and exits without reading data

use strict;
use warnings;

our %args = (
    client => {
	func => sub { errignore(@_); write_char(@_); },
	len => 2**17,
	sndbuf => 2**10,  # small buffer triggers error during write
	# the error message seems to be timing dependent
	down => "Client print failed: (Broken pipe|Connection reset by peer)",
	error => 54,
    },
    relay => {
	func => sub { errignore(@_); relay(@_); },
	rcvbuf => 2**10,
	sndbuf => 2**10,
	down => "Broken pipe",
	nocheck => 1,
	errorin => 0,  # syscall has read the error and resetted it
	errorout => 54,
    },
    server => {
	func => sub { sleep 3; },
	rcvbuf => 2**10,
    },
    len => 131072,
    md5 => "31e5ad3d0d2aeb1ad8aaa847dfa665c2",
    noecho => 1,
);

1;
