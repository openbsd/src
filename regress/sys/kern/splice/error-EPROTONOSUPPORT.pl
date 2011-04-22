#!/usr/bin/perl
# test EPROTONOSUPPORT for splicing udp sockets

use Errno;
use IO::Socket;
use BSD::Socket::Splice "SO_SPLICE";

my $s = IO::Socket::INET->new(
    Proto => "udp",
) or die "socket failed: $!";

my $ss = IO::Socket::INET->new(
    Proto => "udp",
) or die "socket splice failed: $!";

$s->setsockopt(SOL_SOCKET, SO_SPLICE, pack('i', $ss->fileno()))
    and die "splice udp sockets succeeded";
$!{EPROTONOSUPPORT}
    or die "error not EPROTONOSUPPORT: $!"
