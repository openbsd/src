# The TCP server writes cleartext into the TLS connection to syslogd.
# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via IPv4 TLS to an explicit loghost.
# The server accepts an TCP socket.
# Find the message in client, pipe, syslogd log.
# Check that syslogd writes a log message about the SSL connect error.

use strict;
use warnings;
use Socket;

our %args = (
    client => {
	func => sub {
	    my $self = shift;
	    ${$self->{syslogd}}->loggrep("loghost .* connection error", 5)
		or die "no connection error in syslogd.log";
	    write_log($self);
	},
    },
    syslogd => {
	loghost => '@tls://127.0.0.1:$connectport',
	loggrep => {
	    qr/Logging to FORWTLS \@tls:\/\/127.0.0.1:\d+/ => '>=4',
	    get_testlog() => 1,
	    qr/syslogd: loghost .* connection error/ => 2,
	},
    },
    server => {
	listen => { domain => AF_INET, proto => "tcp", addr => "127.0.0.1" },
	func => sub {
	    my $self = shift;
	    print "Writing cleartext into a TLS connection is a bad idea\n";
	    ${$self->{syslogd}}->loggrep("loghost .* connection error", 5)
		or die "no connection error in syslogd.log";
	},
	loggrep => {},
    },
    file => {
	loggrep => {
	    qr/syslogd: loghost .* connection error: connect failed: error:.*/.
		qr/SSL23_GET_SERVER_HELLO:unknown protocol/ => 1,
	},
    },
);

1;
