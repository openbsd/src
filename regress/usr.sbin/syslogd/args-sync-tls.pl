# The client writes 300 long messages to UDP socket.
# The syslogd writes it into a file and through a pipe.
# The syslogd does a TLS reconnect and passes it to loghost.
# The server blocks the message on its TLS socket.
# The server waits until the client has written all messages.
# The server closes the TLS connection and accepts a new one.
# The server receives the messages on its new accepted TLS socket.
# This way the server receives a block of messages that is truncated
# at the beginning and at the end.
# Find the message in client, file, pipe, syslogd, server log.
# Check that the server does not get lines that are cut in the middle.

use strict;
use warnings;
use Socket;

our %args = (
    client => {
	connect => { domain => AF_UNSPEC, addr => "localhost", port => 514 },
	func => sub { write_between2logs(shift, sub {
	    my $self = shift;
	    write_message($self, get_secondlog());
	    write_lines($self, 300, 3000);
	    write_message($self, get_thirdlog());
	    ${$self->{server}}->loggrep("Accepted", 5, 2)
		or die ref($self), " server did not receive second log";
	})},
    },
    syslogd => {
	options => ["-u"],
	loghost => '@tls://127.0.0.1:$connectport',
	loggrep => {
	    get_between2loggrep(),
	    get_charlog() => 300,
	    qr/dropped partial message/ => 1,
	},
    },
    server => {
	listen => { domain => AF_INET, proto => "tls", addr => "127.0.0.1" },
	redo => 0,
	func => sub { read_between2logs(shift, sub {
	    my $self = shift;
	    if ($self->{redo}) {
		$self->{redo}--;
		return;
	    }
	    ${$self->{client}}->loggrep(get_thirdlog(), 5)
		or die ref($self), " client did not send third log";
	    shutdown(\*STDOUT, 1)
		or die "shutdown write failed: $!";
	    $self->{redo}++;
	})},
	loggrep => {
	    qr/Accepted/ => 2,
	    get_between2loggrep(),
	    get_secondlog() => 0,
	    get_thirdlog() => 0,
	    qr/>>> [0-9A-Za-z]{10}/ => 0,
	},
    },
    file => {
	loggrep => {
	    get_between2loggrep(),
	    get_secondlog() => 1,
	    get_thirdlog() => 1,
	    get_charlog() => 300,
	},
    },
);

1;
