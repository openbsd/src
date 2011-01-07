#!/usr/bin/perl
# test EOPNOTSUPP for splicing from listen socket

use Errno;
use IO::Socket;
use constant SO_SPLICE => 0x1023;

my $s = IO::Socket::INET->new(
    Proto => "tcp",
    Listen => 1,
) or die "socket failed: $!";

my $ss = IO::Socket::INET->new(
    Proto => "tcp",
) or die "socket splice failed: $!";

$s->setsockopt(SOL_SOCKET, SO_SPLICE, pack('i', $ss->fileno()))
    and die "splice from listen socket succeeded";
$!{EOPNOTSUPP}
    or die "error not EOPNOTSUPP: $!"
