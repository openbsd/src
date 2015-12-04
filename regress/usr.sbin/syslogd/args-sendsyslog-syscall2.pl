# The client writes a message with sendsyslog2 syscall.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Create a ktrace dump of the client and check that sendsyslog2(2)
# has been used.

use strict;
use warnings;

our %args = (
    client => {
	connect => { domain => "sendsyslog", version => 2, flags => 0 },
	ktrace => {
	    qr/CALL  sendsyslog2\(/ => 2,
	    qr/GIO   fd -1 wrote \d+ bytes/ => 2,
	    qr/RET   sendsyslog2 0/ => 2,
	},
    },
);

1;
