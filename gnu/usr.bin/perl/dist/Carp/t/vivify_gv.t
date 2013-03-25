BEGIN { print "1..2\n"; }

our $has_is_utf8; BEGIN { $has_is_utf8 = exists($utf8::{"is_utf8"}); }
our $has_dgrade; BEGIN { $has_dgrade = exists($utf8::{"downgrade"}); }

use Carp;

print !(exists($utf8::{"is_utf8"}) xor $has_is_utf8) ? "" : "not ", "ok 1\n";
print !(exists($utf8::{"downgrade"}) xor $has_dgrade) ? "" : "not ", "ok 2\n";

1;
