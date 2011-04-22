#!/usr/bin/perl
# test ENOTSOCK for splicing with non-socket

use Errno;
use IO::Socket;
use BSD::Socket::Splice "SO_SPLICE";

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

$s->setsockopt(SOL_SOCKET, SO_SPLICE, pack('i', 0))
    and die "splice with non non-socket fileno succeeded";
$!{ENOTSOCK}
    or die "error not ENOTSOCK: $!"
