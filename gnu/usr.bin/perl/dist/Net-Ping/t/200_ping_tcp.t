use strict;

BEGIN {
  if ($ENV{PERL_CORE}) {
    unless ($ENV{PERL_TEST_Net_Ping}) {
      print "1..0 # Skip: network dependent test\n";
        exit;
    }
  }
  unless (eval "require Socket") {
    print "1..0 \# Skip: no Socket\n";
    exit;
  }
  unless (getservbyname('echo', 'tcp')) {
    print "1..0 \# Skip: no echo port\n";
    exit;
  }
}

# Remote network test using tcp protocol.
#
# NOTE:
#   Network connectivity will be required for all tests to pass.
#   Firewalls may also cause some tests to fail, so test it
#   on a clear network.  If you know you do not have a direct
#   connection to remote networks, but you still want the tests
#   to pass, use the following:
#
# $ PERL_CORE=1 make test

use Test::More tests => 12;
BEGIN {use_ok('Net::Ping');}

my $p = new Net::Ping "tcp",9;

isa_ok($p, 'Net::Ping', 'new() worked');

isnt($p->ping("localhost"), 0, 'Test on the default port');

# Change to use the more common web port.
# This will pull from /etc/services on UNIX.
# (Make sure getservbyname works in scalar context.)
isnt($p->{port_num} = (getservbyname("http", "tcp") || 80), undef);

isnt($p->ping("localhost"), 0, 'Test localhost on the web port');

# Hopefully this is never a routeable host
is($p->ping("172.29.249.249"), 0, "Can't reach 172.29.249.249");

# Test a few remote servers
# Hopefully they are up when the tests are run.

if ($p->ping('google.com')) { # check for firewall
  foreach (qw(google.com www.google.com www.wisc.edu
              yahoo.com www.yahoo.com www.about.com)) {
    isnt($p->ping($_), 0, "Can ping $_");
  }
} else {
 SKIP: {
    skip "Cannot ping google.com: no TCP connection or firewall", 6;
  }
}
