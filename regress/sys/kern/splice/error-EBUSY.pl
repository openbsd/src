#!/usr/bin/perl
# test EBUSY for splicing from a spliced socket

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
) or die "socket connect failed: $!";

my $ss = IO::Socket::INET->new(
    Proto => "tcp",
    PeerAddr => $sl->sockhost(),
    PeerPort => $sl->sockport(),
) or die "socket splice connect failed: $!";

$s->setsockopt(SOL_SOCKET, SO_SPLICE, pack('i', $ss->fileno()))
    or die "splice failed: $!";

my $so = IO::Socket::INET->new(
    Proto => "tcp",
    PeerAddr => $sl->sockhost(),
    PeerPort => $sl->sockport(),
) or die "socket other failed: $!";

$s->setsockopt(SOL_SOCKET, SO_SPLICE, pack('i', $so->fileno()))
    and die "splice from spliced socket succeeded";
$!{EBUSY}
    or die "error not EBUSY: $!"
