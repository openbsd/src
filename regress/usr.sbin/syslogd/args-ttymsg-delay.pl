# The client writes long messages to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe and to tty.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, tty, syslogd, server log.
# Check that syslogd has logged that the tty blocked.

use strict;
use warnings;
use Sys::Syslog qw(:macros);

our %args = (
    client => {
	func => sub {
	    my $self = shift;
	    write_lines($self, 5, 900);
	    write_log($self);
	},
    },
    syslogd => {
	loggrep => {
	    qr/ttymsg delayed write/ => '>=1',
	},
    },
    tty => {
	loggrep => {
	    qr/ 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.* [12]$/ => 2,
	    get_testgrep() => 1,
	},
    },
);

1;
