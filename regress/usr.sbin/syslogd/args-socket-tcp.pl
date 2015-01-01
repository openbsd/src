# Test with default values, that is:
# The client writes a message to a localhost IPv4 TCP socket.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via TCP to the loghost.
# The server receives the message on its TCP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check that the syslogd has one TCP socket in fstat output.

use strict;
use warnings;
use Socket;

our %args = (
    syslogd => {
	fstat => 1,
	loghost => '@tcp://127.0.0.1:$connectport',
	options => ["-n"],
    },
    server => {
	listen => { domain => AF_INET, addr => "127.0.0.1", proto => "tcp" },
    },
    fstat => {
	loggrep => {
	    qr/ internet stream tcp / => 1,
	},
    },
);

1;
