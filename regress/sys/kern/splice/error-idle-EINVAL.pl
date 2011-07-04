#!/usr/bin/perl
# test EINVAL for splicing with negative idle timeout

use Errno;
use IO::Socket;
use BSD::Socket::Splice "SO_SPLICE";
use Config;

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
    PeerAddr => $sl->sockhost(),
    PeerPort => $sl->sockport(),
) or die "socket splice failed: $!";

my $packed;
if ($Config{longsize} == 8) {
    $packed = pack('iiiiiiii', $ss->fileno(),0,0,0,-1,-1,-1,-1);
} else {
    $packed = pack('iiiii', $ss->fileno(),0,0,-1,-1);
}
$s->setsockopt(SOL_SOCKET, SO_SPLICE, $packed)
    and die "splice to unconnected socket succeeded";
$!{EINVAL}
    or die "error not EINVAL: $!"
