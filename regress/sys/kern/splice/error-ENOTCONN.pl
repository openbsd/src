#!/usr/bin/perl
# test ENOTCONN for splicing from unconnected socket

use Errno;
use IO::Socket;
use constant SO_SPLICE => 0x1023;

my $s = IO::Socket::INET->new(
    Proto => "tcp",
) or die "socket failed: $!";

my $ss = IO::Socket::INET->new(
    Proto => "tcp",
) or die "socket splice failed: $!";

$s->setsockopt(SOL_SOCKET, SO_SPLICE, pack('i', $ss->fileno()))
    and die "splice from unconnected socket succeeded";
$!{ENOTCONN}
    or die "error not ENOTCONN: $!"
