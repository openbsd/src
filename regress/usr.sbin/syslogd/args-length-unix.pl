# The client writes long messages to unix domain socket /dev/log.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check that lines in file have 8192 bytes message length after the header.

use strict;
use warnings;
use Socket;

my $msg = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

our %args = (
    client => {
	connect => { domain => AF_UNIX },
	func => \&write_length,
	lengths => [ 8190..8193,9000 ],
    },
    syslogd => {
	loggrep => {
	    $msg => 5,
	}
    },
    file => {
	# Feb  2 00:43:36 hostname 0123456789ABC...567
	loggrep => {
	    $msg => 5,
	    qr/^.{15} \S{1,256} .{8190}$/ => 1,
	    qr/^.{15} \S{1,256} .{8191}$/ => 1,
	    qr/^.{15} \S{1,256} .{8192}$/ => 3,
	},
    },
);

1;
