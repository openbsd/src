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
  if (my $port = getservbyname('echo', 'tcp')) {
    socket(*ECHO, &Socket::PF_INET(), &Socket::SOCK_STREAM(), (getprotobyname 'tcp')[2]);
    unless (connect(*ECHO, scalar &Socket::sockaddr_in($port, &Socket::inet_aton("localhost")))) {
      print "1..0 \# Skip: loopback tcp echo service is off ($!)\n";
      exit;
    }
    close (*ECHO);
  } else {
    print "1..0 \# Skip: no echo port\n";
    exit;
  }
}

# Test of stream protocol using loopback interface.
#
# NOTE:
#   The echo service must be enabled on localhost
#   to really test the stream protocol ping.  See
#   the end of this document on how to enable it.

use Test;
use Net::Ping;
plan tests => 22;

my $p = new Net::Ping "stream";

# new() worked?
ok !!$p;

# Attempt to connect to the echo port
ok ($p -> ping("localhost"));

# Try several pings while it is connected
for (1..20) {
  select (undef,undef,undef,0.1);
  ok $p -> ping("localhost");
}

__END__

A simple xinetd configuration to enable the echo service can easily be made.
Just create the following file before restarting xinetd:

/etc/xinetd.d/echo:

# description: An echo server.
service echo
{
        type            = INTERNAL
        id              = echo-stream
        socket_type     = stream
        protocol        = tcp
        user            = root
        wait            = no
        disable         = no
}


Or if you are using inetd, before restarting, add
this line to your /etc/inetd.conf:

echo   stream  tcp     nowait  root    internal
