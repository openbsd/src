# The client writes a message to Sys::Syslog unix method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check that the file log contains the message.

use strict;
use warnings;

our %args = (
    client => {
	logsock => { type => "unix" },
    },
    file => {
	loggrep => qr/ syslogd-regress\[\d+\]: /. get_log(),
    },
);

1;
