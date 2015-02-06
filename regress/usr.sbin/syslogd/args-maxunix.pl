# The client writes messages to MAXUNIX unix domain sockets.
# The syslogd -a writes them into a file and through a pipe.
# The syslogd -a passes them via UDP to the loghost.
# The server receives the messages on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check that the file log contains a message from every sockets.
# Check that the one socket above the limit prints an error.

use strict;
use warnings;
use IO::Socket::UNIX;
use constant MAXUNIX => 21;

our %args = (
    client => {
	func => sub {
	    my $self = shift;
	    write_unix($self);
	    foreach (1..(MAXUNIX-1)) {
		write_unix($self, "unix-$_.sock");
	    }
	    write_shutdown($self);
	},
    },
    syslogd => {
	options => [ map { ("-a" => "unix-$_.sock") } (1..MAXUNIX) ],
	loggrep => {
	    qr/syslogd: out of descriptors, ignoring unix-20.sock/ => 0,
	    qr/syslogd: out of descriptors, ignoring unix-21.sock/ => 1,
	    qr/syslogd: out of descriptors, ignoring unix-22.sock/ => 0,
	},
    },
    file => {
	loggrep => {
	    get_testlog()." /dev/log unix socket" => 1,
	    (map { (get_testlog()." unix-$_.sock unix socket" => 1) }
		(1..(MAXUNIX-1))),
	    get_testlog()." unix-".MAXUNIX.".sock unix socket" => 0,
	},
    },
);

1;
