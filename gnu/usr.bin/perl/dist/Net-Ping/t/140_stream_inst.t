# Test to make sure object can be instantiated for stream protocol.

use strict;

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

use Test::More tests => 2;
BEGIN {use_ok 'Net::Ping'};

my $p = new Net::Ping "stream";
isa_ok($p, 'Net::Ping', 'object can be instantiated for stream protocol');
