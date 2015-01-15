# The TCP server closes the connection to syslogd.
# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd does a TCP reconnect and passes it to loghost.
# The server receives the message on its new accepted TCP socket.
# Find the message in client, pipe, syslogd, server log.
# Check that syslogd and server close and reopen the connection.

use strict;
use warnings;
use Socket;

our %args = (
    client => {
	func => sub {
	    my $self = shift;
	    write_between2logs($self, sub {
		${$self->{syslogd}}->loggrep("Connection refused", 5)
		    or die "no connection refused in syslogd.log";
	    });
	},
    },
    syslogd => {
	loghost => '@tcp://127.0.0.1:$connectport',
	loggrep => {
	    qr/Logging to FORWTCP \@tcp:\/\/127.0.0.1:\d+/ => '>=6',
	    qr/syslogd: connect .* Connection refused/ => '>=2',
	    get_between2loggrep(),
	},
    },
    server => {
	listen => { domain => AF_INET, proto => "tcp", addr => "127.0.0.1" },
	redo => 0,
	func => sub {
	    my $self = shift;
	    read_between2logs($self, sub {
		if ($self->{redo}) {
		    $self->{redo}--;
		    return;
		}
		$self->close();
		shutdown(\*STDOUT, 1)
		    or die "shutdown write failed: $!";
		${$self->{syslogd}}->loggrep("Connection refused", 5)
		    or die "no connection refused in syslogd.log";
		$self->listen();
		$self->{redo}++;
	    });
	},
	loggrep => {
	    qr/Accepted/ => 2,
	    qr/syslogd: loghost .* connection close/ => 1,
	    qr/syslogd: connect .* Connection refused/ => 1,
	    get_between2loggrep(),
	},
    },
    file => {
	loggrep => {
	    qr/syslogd: connect .* Connection refused/ => '>=1',
	    get_between2loggrep(),
	},
    },
);

1;
