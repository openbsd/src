# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check that a SIGTERM terminates the syslogd child process.

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
	kdump => {
	    qr/syslogd  PSIG  SIGTERM caught handler/ => 1,
	    qr/syslogd  RET   execve 0/ => 1,
	},
	loggrep => qr/\[unpriv\] syslogd child about to exit/,
    },
    server => {
	func => sub {
	    my $self = shift;
	    read_between2logs($self, sub {
		${$self->{syslogd}}->kill_syslogd('TERM');
		my $pattern = "syslogd: exiting on signal 15";
		${$self->{syslogd}}->loggrep("syslogd: exiting on signal 15", 5)
		    or die ref($self),
		    " no 'syslogd: exiting on signal 15' between logs";
		print STDERR "Signal\n";
	    });
	},
	down => qr/syslogd: exiting on signal 15/,
	loggrep => {
	    (get_between2loggrep())[0] => 1,
	    (get_between2loggrep())[2] => 0,
	},
    },
    file => { loggrep => (get_between2loggrep())[0] },
    pipe => { loggrep => (get_between2loggrep())[0] },
);

1;
