# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe and to tty.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.

use strict;
use warnings;
use Sys::Syslog qw(:macros);

our %args = (
    client => {
	func => sub {
	    my $self = shift;
	    syslog(LOG_LOCAL5|LOG_ERR, "test message to all users");
	    write_log($self);
	},
    },
    syslogd => {
	conf => "local5.err\t*",
    },
    tty => {
	loggrep => {
	    qr/Message from syslogd/ => 1,
	    qr/syslogd-regress.* test message to all users/ => 2,
	},
    },
);

1;
