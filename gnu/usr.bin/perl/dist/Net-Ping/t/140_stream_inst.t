# Test to make sure object can be instantiated for stream protocol.

BEGIN {
  unless (eval "require Socket") {
    print "1..0 \# Skip: no Socket\n";
    exit;
  }
  unless (getservbyname('echo', 'tcp')) {
    print "1..0 \# Skip: no echo port\n";
    exit;
  }
}

use Test;
use Net::Ping;
plan tests => 2;

# Everything loaded fine
ok 1;

my $p = new Net::Ping "stream";
ok !!$p;
