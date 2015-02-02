# The client writes long messages to UDP socket.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd log.
# Check that lines with visual encoding at the end are truncated.

use strict;
use warnings;
use Socket;

my $msg = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

our %args = (
    client => {
	connect => { domain => AF_UNSPEC, addr => "localhost", port => 514 },
	func => \&write_length,
	lengths => [ 8186..8195,9000 ],
	tail => "foo\200",
    },
    syslogd => {
	options => ["-u"],
	loggrep => {
	    $msg => 11,
	}
    },
    file => {
	# Jan 31 00:12:39 localhost 0123456789ABC...567
	loggrep => {
	    $msg => 11,
	    qr/^.{25} .{8182}foo\\M\^\@$/ => 1,
	    qr/^.{25} .{8183}foo\\M\^\@$/ => 1,
	    qr/^.{25} .{8184}foo\\M\^\@$/ => 1,
	    qr/^.{25} .{8185}foo\\M\^\@$/ => 1,
	    qr/^.{25} .{8186}foo\\M\^$/ => 1,
	    qr/^.{25} .{8187}foo\\M$/ => 1,
	    qr/^.{25} .{8188}foo\\$/ => 1,
	    qr/^.{25} .{8189}foo$/ => 1,
	    qr/^.{25} .{8190}fo$/ => 1,
	    qr/^.{25} .{8191}f$/ => 1,
	    qr/^.{25} .{8192}$/ => 8,
	},
    },
);

1;
