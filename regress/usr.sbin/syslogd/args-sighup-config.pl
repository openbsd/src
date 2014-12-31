# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check that a modified config file restarts syslogd child.

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
	    qr/syslogd  PSIG  SIGHUP caught handler/ => 1,
	    qr/syslogd  RET   execve 0/ => 2,
	},
	loggrep => {
	    qr/config file modified: restarting/ => 1,
	    qr/config file changed: dying/ => 1,
	    qr/syslogd: restarted/ => 0,
	    get_between2loggrep(),
	},
    },
    server => {
	func => sub {
	    my $self = shift;
	    read_between2logs($self, sub {
		my $conffile = ${$self->{syslogd}}->{conffile};
		open(my $fh, '>>', $conffile)
		    or die ref($self), " append conf file $conffile failed: $!";
		print $fh "# modified\n";
		close($fh);
		${$self->{syslogd}}->kill_syslogd('HUP');
		${$self->{syslogd}}->loggrep("syslogd: started", 5)
		    or die ref($self), " no 'syslogd: started' between logs";
		print STDERR "Signal\n";
	    });
	},
	loggrep => { get_between2loggrep() },
    },
);

1;
