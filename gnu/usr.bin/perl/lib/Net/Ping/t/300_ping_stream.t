BEGIN {
  if ($ENV{PERL_CORE}) {
    unless ($ENV{PERL_TEST_Net_Ping}) {
      print "1..0 # Skip: network dependent test\n";
        exit;
    }
    chdir 't' if -d 't';
    @INC = qw(../lib);
  }
  unless (eval "require Socket") {
    print "1..0 \# Skip: no Socket\n";
    exit;
  }
  unless (getservbyname('echo', 'udp')) {
    print "1..0 \# Skip: no echo port\n";
    exit;
  }
}

# Test of stream protocol using loopback interface.
#
# NOTE:
#   The echo service must be enabled on localhost
#   to really test the stream protocol ping.

use Test;
use Net::Ping;
plan tests => 12;

my $p = new Net::Ping "stream";

# new() worked?
ok !!$p;

# Attempt to connect to the echo port
if ($p -> ping("localhost")) {
  ok 1;
  # Try several pings while it is connected
  for (1..10) {
    ok $p -> ping("localhost");
  }
} else {
  # Echo port is off, skip the tests
  for (2..12) { skip "Local echo port is off", 1; }
  exit;
}

__END__

A simple xinetd configuration to enable the echo service can easily be made.
Just create the following file before restarting xinetd:

/etc/xinetd.d/echo:

# description: echo service
service echo
{
        socket_type             = stream
        wait                    = no
        user                    = root
        server                  = /bin/cat
        disable                 = no
}

Or if you are using inetd, before restarting, add
this line to your /etc/inetd.conf:

echo   stream  tcp     nowait  root    internal
