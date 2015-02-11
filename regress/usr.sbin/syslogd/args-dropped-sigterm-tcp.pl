# The client writes 300 messages to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via TCP to the loghost.
# The server blocks the message on its TCP socket.
# The server waits until the client as written all messages.
# The server sends a SIGTERM to syslogd and reads messages from kernel.
# The client waits until the server has read the first message.
# Find the message in client, file, pipe, syslogd log.
# Check that the 300 messages are in syslogd and file log.
# Check that the dropped message is in file log.

use strict;
use warnings;
use Socket;

our %args = (
    client => {
	func => sub { write_between2logs(shift, sub {
	    my $self = shift;
	    write_message($self, get_secondlog());
	    write_lines($self, 300, 1024);
	    write_message($self, get_thirdlog());
	    ${$self->{server}}->loggrep(get_secondlog(), 8)
		or die ref($self), " server did not receive second log";
	})},
    },
    syslogd => {
	loghost => '@tcp://localhost:$connectport',
	loggrep => {
	    get_charlog() => 300,
	    qr/ \(dropped\)/ => 17,
	},
    },
    server => {
	listen => { domain => AF_UNSPEC, proto => "tcp", addr => "localhost" },
	redo => 0,
	func => sub {
	    my $self = shift;
	    ${$self->{client}}->loggrep(get_thirdlog(), 20)
		or die ref($self), " client did not send third log";
	    ${$self->{syslogd}}->kill_syslogd('TERM');
	    ${$self->{syslogd}}->loggrep("syslogd: exiting", 5)
		or die ref($self), " no 'syslogd: exiting' between logs";
	    # syslogd has shut down, read from kernel socket buffer
	    read_log($self);
	},
	loggrep => {
	    get_firstlog() => 1,
	    get_secondlog() => 1,
	    get_thirdlog() => 0,
	    get_testlog() => 0,
	    qr/syslogd: start/ => 1,
	    get_charlog() => 42,
	},
    },
    pipe => {
	loggrep => {},
    },
    file => {
	loggrep => {
	    get_firstlog() => 1,
	    get_secondlog() => 1,
	    get_thirdlog() => 1,
	    get_testlog() => 0,
	    qr/syslogd: start/ => 1,
	    get_charlog() => 300,
	    qr/syslogd: dropped 260 messages to remote loghost/ => 1,
	},
    },
);

1;
