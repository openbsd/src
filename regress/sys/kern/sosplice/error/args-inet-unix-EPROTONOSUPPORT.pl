# test EPROTONOSUPPORT for splicing inet and unix sockets

use strict;
use warnings;
use IO::Socket;
use BSD::Socket::Splice "SO_SPLICE";
use IO::Socket::UNIX;

our %args = (
    errno => 'EPROTONOSUPPORT',
    func => sub {
	my $s = IO::Socket::INET->new(
	    Proto => "udp",
	    LocalAddr => "127.0.0.1",
	) or die "socket bind failed: $!";

	my $ss = IO::Socket::UNIX->new(
	    Type => SOCK_STREAM,
	) or die "socket splice failed: $!";

	$s->setsockopt(SOL_SOCKET, SO_SPLICE, pack('i', $ss->fileno()))
	    and die "splice inet and unix sockets succeeded";
    },
);
