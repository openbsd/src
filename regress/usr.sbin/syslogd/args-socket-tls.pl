# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via TLS to the loghost.
# The server receives the message on its TLS socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check that the syslogd has one TCP socket in fstat output.

use strict;
use warnings;
use Socket;

our %args = (
    syslogd => {
	fstat => 1,
	loghost => '@tls://127.0.0.1:$connectport',
	options => ["-n"],
    },
    server => {
	listen => { domain => AF_INET, addr => "127.0.0.1", proto => "tls" },
    },
    fstat => {
	loggrep => {
	    qr/ internet stream tcp / => 1,
	},
    },
);

1;
