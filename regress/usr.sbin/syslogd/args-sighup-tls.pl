# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via TLS to the loghost.
# The server receives the message on its TLS socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check that a SIGHUP reconnects the TLS stream and closes the socket.

use strict;
use warnings;

our %args = (
    client => {
	func => sub {
	    my $self = shift;
	    write_between2logs($self, sub {
		${$self->{server}}->loggrep("Signal", 8)
		    or die ref($self), " no 'Signal' between logs";
	    });
	},
	loggrep => { get_between2loggrep() },
    },
    syslogd => {
	ktrace => 1,
	fstat => 1,
	kdump => {
	    qr/syslogd  PSIG  SIGHUP caught handler/ => 1,
	    qr/syslogd  RET   execve 0/ => 1,
	},
	loghost => '@tls://127.0.0.1:$connectport',
	loggrep => {
	    qr/config file changed: dying/ => 0,
	    qr/config file modified: restarting/ => 0,
	    qr/syslogd: restarted/ => 1,
	    get_between2loggrep(),
	},
    },
    server => {
	listen => { domain => AF_INET, addr => "127.0.0.1", proto => "tls" },
	redo => 0,
	func => sub {
	    my $self = shift;
	    read_between2logs($self, sub {
		if ($self->{redo}) {
			$self->{redo}--;
			return;
		}
		${$self->{syslogd}}->rotate();
		${$self->{syslogd}}->kill_syslogd('HUP');
		${$self->{syslogd}}->loggrep("syslogd: restarted", 5)
		    or die ref($self), " no 'syslogd: restarted' between logs";
		print STDERR "Signal\n";
		# regeneate fstat file
		${$self->{syslogd}}->fstat();
		$self->{redo}++;
	    });
	},
	loggrep => {
	    get_between2loggrep(),
	    qr/Signal/ => 1,
	    qr/Accepted/ => 2,
	},
    },
    fstat => {
	loggrep => {
	    # sighup must not leak a TCP socket
	    qr/internet stream tcp/ => 1,
	},
    },
);

1;
