#!/usr/bin/perl
# test EBADF for splicing with non existing fileno

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

$s->setsockopt(SOL_SOCKET, SO_SPLICE, pack('i', 23))
    and die "splice with non existing fileno succeeded";
$!{EBADF}
    or die "error not EBADF: $!"
