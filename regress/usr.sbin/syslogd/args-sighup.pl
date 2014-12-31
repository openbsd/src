# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check that a SIGHUP reopens logfile and restarts pipe.

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
	    qr/syslogd  RET   execve 0/ => 1,
	},
	loggrep => {
	    qr/config file changed: dying/ => 0,
	    qr/config file modified: restarting/ => 0,
	    qr/syslogd: restarted/ => 1,
	    get_between2loggrep(),
	},
    },
    server => {
	func => sub {
	    my $self = shift;
	    read_between2logs($self, sub {
		${$self->{syslogd}}->rotate();
		${$self->{syslogd}}->kill_syslogd('HUP');
		${$self->{syslogd}}->loggrep("syslogd: restarted", 5)
		    or die ref($self), " no 'syslogd: restarted' between logs";
		print STDERR "Signal\n";
	    });
	},
	loggrep => { get_between2loggrep() },
    },
    check => sub {
	my $self = shift;
	my $r = $self->{syslogd};
	foreach my $name (qw(file pipe)) {
		my $file = $r->{"out$name"}.".0";
		my $pattern = (get_between2loggrep())[0];
		check_pattern($name, $file, $pattern, \&filegrep);
	}
    },
);

1;
