# The client writes messages to localhost IPv4 and IPv6 UDP socket.
# The syslogd does not receive them as it is started without -u.
# Check that client does send the message, but it is not in the file.
# Check with fstat that both *:514 sockets are bound.
# Check that there is no recvfrom localhost in syslogd ktrace.

use strict;
use warnings;
use Socket;

our %args = (
    client => {
	connectaddr => "none",
	redo => [
	    {
		domain => AF_INET,
		addr   => "127.0.0.1",
	    },
	    {
		domain => AF_INET,
		addr   => "127.0.0.1",
	    },
	    {
		domain => AF_INET6,
		addr   => "::1",
	    },
	],
	func => sub {
	    my $self = shift;
	    write_message($self, "client addr: ". $self->{connectaddr});
	    if ($self->{cs}) {
		# wait for possible icmp errors, port is open
		sleep .1;
		close($self->{cs})
		    or die ref($self), " close failed: $!";
	    };
	    if (my $connect = shift @{$self->{redo}}) {
		$self->{connectdomain} = $connect->{domain};
		$self->{connectaddr}   = $connect->{addr};
		$self->{connectproto}  = "udp";
		$self->{connectport}   = "514";
	    } else {
		delete $self->{connectdomain};
		$self->{logsock} = { type => "native" };
		setlogsock($self->{logsock})
		    or die ref($self), " setlogsock failed: $!";
		sleep .1;
		write_log($self);
		undef $self->{redo};
	    }
	},
	loggrep => {
	    qr/client addr:/ => 4,
	    get_testgrep() => 1,
	}
    },
    syslogd => {
	options => [],
	loghost => "/dev/null",
	fstat => {
	    qr/^_syslogd syslogd .* internet6? dgram udp \*:514$/ => 2,
	},
	ktrace => {
	    qr/127\.0\.0\.1/ => 0,
	    qr/\[::1\]/ => 0,
	},
    },
    server => {
	noserver => 1,
    },
    file => {
	loggrep => {
	    qr/client addr: none/ => 1,
	    qr/client addr:/ => 1,
	    get_testgrep() => 1,
	}
    },
);

1;
