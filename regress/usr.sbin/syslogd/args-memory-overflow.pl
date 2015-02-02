# The client writes message to overflow the memory buffer method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Syslogc checks the memory logs.
# Check that memory buffer has overflow flag.

use strict;
use warnings;

our %args = (
    client => {
	func => sub {
	    my $self = shift;
	    foreach (1..40) {
		write_message($self, $_ x 1024);
	    }
	    write_log($self);
	},
    },
    syslogd => {
	memory => 1,
	loggrep => {
	    qr/Accepting control connection/ => 5,
	    qr/ctlcmd 1/ => 1,  # read
	    qr/ctlcmd 2/ => 1,  # read clear
	    qr/ctlcmd 4/ => 3,  # list
	},
    },
    syslogc => [ {
	options => ["-q"],
	loggrep => qr/^memory\* /,
    }, {
	options => ["memory"],
	down => get_downlog(),
	loggrep => {},
    }, {
	options => ["-q"],
	loggrep => qr/^memory\* /,
    }, {
	options => ["-c", "memory"],
	down => get_downlog(),
	loggrep => {},
    }, {
	options => ["-q"],
	loggrep => qr/^memory /,
    } ],
);

1;
