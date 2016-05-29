# The TLS server closes the connection to syslogd.
# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd does a TLS reconnect and passes it to loghost.
# The server receives the message on its new accepted TLS socket.
# Find the message in client, pipe, syslogd, server log.
# Check that syslogd and server close and reopen the connection.

use strict;
use warnings;
use Socket;
use Errno ':POSIX';

my @errors = (ECONNREFUSED, EPIPE);
my $errors = "(". join("|", map { $! = $_ } @errors). ")";

our %args = (
    client => {
	func => sub { write_between2logs(shift, sub {
	    my $self = shift;
	    ${$self->{syslogd}}->loggrep($errors, 5)
		or die "no $errors in syslogd.log";
	})},
    },
    syslogd => {
	loghost => '@tls://127.0.0.1:$connectport',
	loggrep => {
	    qr/Logging to FORWTLS \@tls:\/\/127.0.0.1:\d+/ => '>=6',
	    qr/syslogd: (connect .*|.* connection error: handshake failed): /.
		$errors => '>=2',
	    get_between2loggrep(),
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
	    $self->close();
	    shutdown(\*STDOUT, 1)
		or die "shutdown write failed: $!";
	    ${$self->{syslogd}}->loggrep($errors, 5)
		or die "no $errors in syslogd.log";
	    $self->listen();
	    $self->{redo}++;
	})},
	loggrep => {
	    qr/Accepted/ => 2,
	    qr/syslogd: loghost .* connection close/ => 1,
	    qr/syslogd: (connect .*|.* connection error: handshake failed): /.
		$errors => 1,
	    get_between2loggrep(),
	},
    },
    file => {
	loggrep => {
	    qr/syslogd: (connect .*|.* connection error: handshake failed): /.
		$errors => '>=1',
	    get_between2loggrep(),
	},
    },
);

1;
