BEGIN { print "1..1\n"; }

our $has_utf8; BEGIN { $has_utf8 = exists($::{"utf8::"}); }

use Carp;

print !(exists($::{"utf8::"}) xor $has_utf8) ? "" : "not ", "ok 1\n";

1;
