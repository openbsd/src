# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via TLS to localhost loghost.
# The server offers only the null cipher on its TLS socket.
# Find the message in client, file, pipe, syslogd log.
# Check that server log contains the no shared cipher error.

use strict;
use warnings;
use Socket;

our %args = (
    syslogd => {
	loghost => '@tls://localhost:$connectport',
	loggrep => {
	    qr/Logging to FORWTLS \@tls:\/\/localhost:\d+/ => '>=4',
	    qr/syslogd: loghost .* connection error: connect failed: error.*:/.
		qr/SSL23_GET_SERVER_HELLO:sslv3 alert handshake failure/ => 2,
	    get_testgrep() => 1,
	},
	cacrt => "ca.crt",
    },
    server => {
	listen => { domain => AF_UNSPEC, proto => "tls", addr => "localhost" },
	sslciphers => "NULL",
	up => "IO::Socket::SSL socket accept failed",
	down => "Server",
	exit => 255,
	loggrep => {
	    qr/listen sock: (127.0.0.1|::1) \d+/ => 1,
	    qr/no shared cipher/ => 1,
	    get_testgrep() => 0,
	},
    },
);

1;
