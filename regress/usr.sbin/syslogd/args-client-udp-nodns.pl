# The client writes a message to Sys::Syslog UDP method.
# The syslogd writes it into a file and through a pipe without dns.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check that the file log contains the 127.0.0.1 address.

use strict;
use warnings;

our %args = (
    client => {
	logsock => { type => "udp", host => "127.0.0.1", port => 514 },
    },
    syslogd => {
	options => ["-un"],
    },
    file => {
	loggrep => qr/ 127.0.0.1 syslogd-regress\[\d+\]: /. get_log(),
    },
);

1;
