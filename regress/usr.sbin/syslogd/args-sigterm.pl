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
	    write_message($self, get_testlog());
	    ${$self->{server}}->loggrep("syslogd: exiting", 8)
		or die ref($self), " no 'syslogd: exiting' server log";
	},
    },
    syslogd => {
	ktrace => {
	    qr/syslogd  PSIG  SIGTERM caught handler/ => 1,
	    qr/syslogd  RET   execve 0/ => 1,
	},
	loggrep => qr/\[unpriv\] syslogd child about to exit/,
    },
    server => {
	func => sub {
	    my $self = shift;
	    read_message($self, get_testgrep());
	    ${$self->{syslogd}}->kill_syslogd('TERM');
	    read_message($self, qr/syslogd: exiting/);
	},
	down => qr/syslogd: exiting on signal 15/,
    },
);

1;
