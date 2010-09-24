# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.t'

######################### We start with some black magic to print on failure.

BEGIN {
  unless (eval "require Socket") {
    print "1..0 \# Skip: no Socket\n";
    exit;
  }
}

use Test;
BEGIN { plan tests => 1; $loaded = 0}
END { ok $loaded;}

# Just make sure everything compiles
use Net::Ping;

$loaded = 1;

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):
