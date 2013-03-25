use strict;

BEGIN {
  unless (eval "require Socket") {
    print "1..0 \# Skip: no Socket\n";
    exit;
  }
}

use Test::More tests => 1;
# Just make sure everything compiles
BEGIN {use_ok 'Net::Ping'};
