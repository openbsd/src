# The syslogd listens on 127.0.0.1 TCP socket.
# The client writes long line into a 127.0.0.1 TCP socket.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in file, syslogd, server log.
# Check that the file log contains the truncated message.

use strict;
use warnings;
use constant MAXLINE => 8192;
use constant MAX_UDPMSG => 1180;

our %args = (
    client => {
	connect => { domain => AF_INET, proto => "tcp", addr => "127.0.0.1",
	    port => 514 },
	func => sub {
	    my $self = shift;
	    local $| = 1;
	    my $msg = generate_chars(MAXLINE+1);
	    print $msg;
	    print STDERR "<<< $msg\n";
	    ${$self->{syslogd}}->loggrep("tcp logger .* incomplete line", 5)
		or die ref($self), " syslogd did not receive incomplete line";
	    write_shutdown($self);
	},
	loggrep => {},
    },
    syslogd => {
	options => ["-T", "127.0.0.1:514"],
	loggrep => qr/incomplete line, use /.(MAXLINE+1).qr/ bytes/,
    },
    server => {
	# >>> <13>Jul  6 22:33:32 0123456789ABC...fgh
	loggrep => qr/>>> .{19} /.generate_chars(MAX_UDPMSG-20).qr/$/,
    },
    file => {
	loggrep => generate_chars(MAXLINE).qr/$/,
    },
    pipe => { loggrep => {} },  # XXX syslogd ignore short writes to pipe
);

1;
