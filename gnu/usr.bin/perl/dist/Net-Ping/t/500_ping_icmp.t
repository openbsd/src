# Test to perform icmp protocol testing.
# Root access is required.

use strict;
use Config;

use Test::More;
BEGIN {
  unless (eval "require Socket") {
    plan skip_all => 'no Socket';
  }
  unless ($Config{d_getpbyname}) {
    plan skip_all => 'no getprotobyname';
  }
}

BEGIN {use_ok('Net::Ping')};

SKIP: {
  skip "icmp ping requires root privileges.", 1
    if !Net::Ping::_isroot() or $^O eq 'MSWin32';
  my $p = new Net::Ping "icmp";
  my $result = $p->ping("127.0.0.1");
  if ($result == 1) {
    is($result, 1, "icmp ping 127.0.0.1");
  } else {
  TODO: {
      local $TODO = "icmp firewalled?";
      is($result, 1, "icmp ping 127.0.0.1");
    }
  }
}

done_testing;
