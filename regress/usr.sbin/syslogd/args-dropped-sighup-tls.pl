# The client writes 300 messages to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via TLS to the loghost.
# The server blocks the message on its TLS socket.
# The server waits until the client as written all messages.
# The server sends a SIGHUP to syslogd and reads messages from kernel.
# The client waits until the server has read the first message.
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
	    write_lines($self, 300, 1024);
	    write_message($self, get_thirdlog());
	    ${$self->{server}}->loggrep(get_secondlog(), 8)
		or die ref($self), " server did not receive second log";
	})},
    },
    syslogd => {
	loghost => '@tls://localhost:$connectport',
	loggrep => {
	    get_between2loggrep(),
	    get_charlog() => 300,
	    qr/ \(dropped\)/ => '~16',
	},
    },
    server => {
	listen => { domain => AF_UNSPEC, proto => "tls", addr => "localhost" },
	redo => 0,
	func => sub { read_between2logs(shift, sub {
	    my $self = shift;
	    if ($self->{redo}) {
		    $self->{redo}--;
		    return;
	    }
	    ${$self->{client}}->loggrep(get_thirdlog(), 20)
		or die ref($self), " client did not send third log";
	    ${$self->{syslogd}}->kill_syslogd('HUP');
	    ${$self->{syslogd}}->loggrep("syslogd: restarted", 5)
		or die ref($self), " no 'syslogd: restarted' between logs";
	    # syslogd has shut down, read from kernel socket buffer
	    read_log($self);
	    $self->{redo}++;
	})},
	loggrep => {
	    get_between2loggrep(),
	    get_secondlog() => 1,
	    get_thirdlog() => 0,
	    qr/syslogd: start/ => 1,
	    qr/syslogd: restart/ => 1,
	    get_charlog() => 41,
	    qr/syslogd: dropped 2[56][0-9] messages to remote loghost/ => 1,
	},
    },
    file => {
	loggrep => {
	    get_between2loggrep(),
	    get_secondlog() => 1,
	    get_thirdlog() => 1,
	    qr/syslogd: start/ => 1,
	    qr/syslogd: restart/ => 1,
	    get_charlog() => 300,
	    qr/syslogd: dropped 2[56][0-9] messages to remote loghost/ => 1,
	},
    },
);

1;
