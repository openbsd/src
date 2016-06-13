# test ELOOP for udp splicing loop

use strict;
use warnings;
use IO::Socket;
use BSD::Socket::Splice "SO_SPLICE";

our %args = (
    errno => 'ELOOP',
    func => sub {
	my $s = IO::Socket::INET->new(
	    Proto => "udp",
	    LocalAddr => "127.0.0.1",
	) or die "socket bind failed: $!";

	my $ss = IO::Socket::INET->new(
	    Proto => "udp",
	    PeerAddr => $s->sockhost(),
	    PeerPort => $s->sockport(),
	) or die "socket connect failed: $!";

	$s->setsockopt(SOL_SOCKET, SO_SPLICE, pack('i', $ss->fileno()))
	    or die "splice failed: $!";

	defined($ss->send("foo\n"))
	    or die "socket splice send failed: $!";
	defined($s->recv(my $buf, 10))
	    or die "socket recv failed: $!";
	$buf eq "foo\n"
	    or die "socket recv unexpected content: $buf";
	defined($s->recv($buf, 10))
	    and die "socket recv succeeded";
    },
);
