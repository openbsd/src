# test connection reset by server

use strict;
use warnings;

our %args = (
    client => {
	func => sub { errignore(@_); write_char(@_); },
	len => 2**17,
    },
    relay => {
	func => sub { errignore(@_); relay(@_); },
	sndbuf => 2**12,
	rcvbuf => 2**12,
	down => "Broken pipe|Connection reset by peer",
    },
    server => {
	func => sub { sleep 3; solinger(@_); },
	rcvbuf => 2**12,
    },
    nocheck => 1,
);

1;
