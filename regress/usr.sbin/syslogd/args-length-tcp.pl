# The client writes long messages to UDP socket.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via TCP to the loghost.
# The server receives the message on its TCP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check that lines in server have 8192 bytes message length.

use strict;
use warnings;
use Socket;

my $msg = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

our %args = (
    client => {
	connect => { domain => AF_UNSPEC, addr => "localhost", port => 514 },
	func => \&write_length,
	lengths => [ 8190..8193,9000 ],
    },
    syslogd => {
	loghost => '@tcp://localhost:$connectport',
	options => ["-u"],
	loggrep => {
	    $msg => 5,
	}
    },
    server => {
	listen => { domain => AF_UNSPEC, proto => "tcp", addr => "localhost" },
	# >>> <13>Jan 31 00:10:11 0123456789ABC...567
	loggrep => {
	    $msg => 5,
	    qr/^>>> .{19} .{8190}$/ => 1,
	    qr/^>>> .{19} .{8191}$/ => 1,
	    qr/^>>> .{19} .{8192}$/ => 3,
	},
    },
);

1;
