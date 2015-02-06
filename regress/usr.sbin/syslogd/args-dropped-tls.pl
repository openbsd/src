# The client writes 300 messages to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via TLS to the loghost.
# The server blocks the message on its TLS socket.
# The server waits until the client has written all messages.
# The server receives the message on its TLS socket.
# The client waits until the server as read the first message.
# Find the message in client, file, pipe, syslogd, server log.
# Check that the 300 messages are in syslogd and file log.
# Check that the dropped message is in server and file log.

use strict;
use warnings;
use Socket;

our %args = (
    client => {
	func => sub { write_between2logs(shift, sub {
	    my $self = shift;
	    write_message($self, get_secondlog());
	    foreach (1..300) {
		write_char($self, [1024], $_);
		# if client sends too fast, syslogd will not see everything
		sleep .01;
	    }
	    write_message($self, get_thirdlog());
	    ${$self->{server}}->loggrep(get_secondlog(), 5)
		or die ref($self), " server did not receive second log";
	})},
    },
    syslogd => {
	loghost => '@tls://localhost:$connectport',
	loggrep => {
	    get_between2loggrep(),
	    get_charlog() => 300,
	},
    },
    server => {
	listen => { domain => AF_UNSPEC, proto => "tls", addr => "localhost" },
	func => sub {
	    my $self = shift;
	    ${$self->{client}}->loggrep(get_thirdlog(), 5)
		or die ref($self), " client did not send third log";
	    read_log($self);
	},
	loggrep => {
	    get_between2loggrep(),
	    get_secondlog() => 1,
	    get_thirdlog() => 0,
	    get_charlog() => 288,
	    qr/syslogd: dropped 13 messages to loghost "\@tls:.*"/ => 1,
	},
    },
    file => {
	loggrep => {
	    get_between2loggrep(),
	    get_secondlog() => 1,
	    get_thirdlog() => 1,
	    get_charlog() => 300,
	    qr/syslogd: dropped 13 messages to loghost "\@tls:.*"/ => 1,
	},
    },
);

1;
