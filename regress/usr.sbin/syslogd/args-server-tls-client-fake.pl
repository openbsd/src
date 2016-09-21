# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via TLS with client certificate to the loghost.
# The server tries to verify the connection to its TLS socket with wrong ca.
# Find the message in client, file, pipe, syslogd log.
# Check that syslogd and server have error message in log.

use strict;
use warnings;
use Socket;

our %args = (
    syslogd => {
	options => [qw(-c client.crt -k client.key)],
	loghost => '@tls://localhost:$connectport',
	loggrep => {
	    qr/ClientCertfile client.crt/ => 1,
	    qr/ClientKeyfile client.key/ => 1,
	    qr/syslogd: loghost .* connection error: /.
		qr/handshake failed: error:.*/.
		qr/SSL3_READ_BYTES:tlsv1 alert decrypt error/ => 2,
	    get_testgrep() => 1,
	},
    },
    server => {
	listen => { domain => AF_UNSPEC, proto => "tls", addr => "localhost" },
	sslca => "fake-ca.crt",
	up => qr/IO::Socket::SSL socket accept failed/,
	down => qr/SSL accept attempt failed error/,
	exit => 255,
	loggrep => {
	    qr/Server IO::Socket::SSL socket accept failed: /.
		qr/,SSL accept attempt failed error:.*/.
		qr/SSL3_GET_CLIENT_CERTIFICATE:no certificate returned/ => 1.
	},
    },
);

1;
