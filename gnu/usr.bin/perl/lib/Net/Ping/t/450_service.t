# Testing service_check method using tcp and syn protocols.

BEGIN {
  unless (eval "require IO::Socket") {
    print "1..0 \# Skip: no IO::Socket\n";
    exit;
  }
  unless (getservbyname('echo', 'tcp')) {
    print "1..0 \# Skip: no echo port\n";
    exit;
  }
}

use strict;
use Test;
use Net::Ping;

# I'm lazy so I'll just use IO::Socket
# for the TCP Server stuff instead of doing
# all that direct socket() junk manually.

plan tests => 26, ($^O eq 'MSWin32' ? (todo => [18]) : ());

# Everything loaded fine
ok 1;

# Start a tcp listen server on ephemeral port
my $sock1 = new IO::Socket::INET
  LocalAddr => "127.0.0.1",
  Proto => "tcp",
  Listen => 8,
  or warn "bind: $!";

# Make sure it worked.
ok !!$sock1;

# Start listening on another ephemeral port
my $sock2 = new IO::Socket::INET
  LocalAddr => "127.0.0.1",
  Proto => "tcp",
  Listen => 8,
  or warn "bind: $!";

# Make sure it worked too.
ok !!$sock2;

my $port1 = $sock1->sockport;
ok $port1;

my $port2 = $sock2->sockport;
ok $port2;

# Make sure the sockets are listening on different ports.
ok ($port1 != $port2);

$sock2->close;

# This is how it should be:
# 127.0.0.1:$port1 - service ON
# 127.0.0.1:$port2 - service OFF

#####
# First, we test using the "tcp" protocol.
# (2 seconds should be long enough to connect to loopback.)
my $p = new Net::Ping "tcp", 2;

# new() worked?
ok !!$p;

# Disable service checking
$p->service_check(0);

# Try on the first port
$p->{port_num} = $port1;

# Make sure it is reachable
ok $p -> ping("127.0.0.1");

# Try on the other port
$p->{port_num} = $port2;

# Make sure it is reachable
ok $p -> ping("127.0.0.1");



# Enable service checking
$p->service_check(1);

# Try on the first port
$p->{port_num} = $port1;

# Make sure service is on
ok $p -> ping("127.0.0.1");

# Try on the other port
$p->{port_num} = $port2;

# Make sure service is off
ok !$p -> ping("127.0.0.1");

# test 11 just finished.

#####
# Lastly, we test using the "syn" protocol.
$p = new Net::Ping "syn", 2;

# new() worked?
ok !!$p;

# Disable service checking
$p->service_check(0);

# Try on the first port
$p->{port_num} = $port1;

# Send SYN
if (!ok $p -> ping("127.0.0.1")) {warn "ERRNO: $!";}

# IP should be reachable
ok $p -> ack();
# No more sockets?
ok !$p -> ack();

###
# Get a fresh object
$p = new Net::Ping "syn", 2;

# new() worked?
ok !!$p;

# Disable service checking
$p->service_check(0);

# Try on the other port
$p->{port_num} = $port2;

# Send SYN
if (!ok $p -> ping("127.0.0.1")) {warn "ERRNO: $!";}

# IP should still be reachable
ok $p -> ack();
# No more sockets?
ok !$p -> ack();


###
# Get a fresh object
$p = new Net::Ping "syn", 2;

# new() worked?
ok !!$p;

# Enable service checking
$p->service_check(1);

# Try on the first port
$p->{port_num} = $port1;

# Send SYN
ok $p -> ping("127.0.0.1");

# Should have service on
ok ($p -> ack(),"127.0.0.1");
# No more good sockets?
ok !$p -> ack();


###
# Get a fresh object
$p = new Net::Ping "syn", 2;

# new() worked?
ok !!$p;

# Enable service checking
$p->service_check(1);

# Try on the other port
$p->{port_num} = $port2;

# Send SYN
if (!ok $p -> ping("127.0.0.1")) {warn "ERRNO: $!";}

# No sockets should have service on
ok !$p -> ack();
