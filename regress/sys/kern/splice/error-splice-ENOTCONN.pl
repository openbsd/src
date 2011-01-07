#!/usr/bin/perl
# test ENOTCONN for splicing to unconnected socket

use Errno;
use IO::Socket;
use constant SO_SPLICE => 0x1023;

my $sl = IO::Socket::INET->new(
    Proto => "tcp",
    Listen => 5,
    LocalAddr => "127.0.0.1",
) or die "socket listen failed: $!";

my $s = IO::Socket::INET->new(
    Proto => "tcp",
    PeerAddr => $sl->sockhost(),
    PeerPort => $sl->sockport(),
) or die "socket failed: $!";

my $ss = IO::Socket::INET->new(
    Proto => "tcp",
) or die "socket splice failed: $!";

$s->setsockopt(SOL_SOCKET, SO_SPLICE, pack('i', $ss->fileno()))
    and die "splice to unconnected socket succeeded";
$!{ENOTCONN}
    or die "error not ENOTCONN: $!"
