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

use Test;
use Net::Ping;
plan tests => 13;

# Everything loaded fine
ok 1;

my $p = new Net::Ping "tcp",9;

# new() worked?
ok !!$p;

# Test on the default port
ok $p -> ping("localhost");

# Change to use the more common web port.
# This will pull from /etc/services on UNIX.
# (Make sure getservbyname works in scalar context.)
ok ($p -> {port_num} = (getservbyname("http", "tcp") || 80));

# Test localhost on the web port
ok $p -> ping("localhost");

# Hopefully this is never a routeable host
ok !$p -> ping("172.29.249.249");

# Test a few remote servers
# Hopefully they are up when the tests are run.

ok $p -> ping("www.geocities.com");
ok $p -> ping("ftp.geocities.com");

ok $p -> ping("www.freeservers.com");
ok $p -> ping("ftp.freeservers.com");

ok $p -> ping("yahoo.com");
ok $p -> ping("www.yahoo.com");
ok $p -> ping("www.about.com");
