BEGIN { print "1..5\n"; }

our $has_utf8; BEGIN { $has_utf8 = exists($::{"utf8::"}); }
our $has_overload; BEGIN { $has_overload = exists($::{"overload::"}); }
our $has_B; BEGIN { $has_B = exists($::{"B::"}); }

use Carp;
sub { sub { Carp::longmess("x") }->() }->(\1, "\x{2603}", qr/\x{2603}/);

print !(exists($::{"utf8::"}) xor $has_utf8) ? "" : "not ", "ok 1\n";
print !(exists($::{"overload::"}) xor $has_overload) ? "" : "not ", "ok 2\n";
print !(exists($::{"B::"}) xor $has_B) ? "" : "not ", "ok 3\n";

# Autovivify $::{"overload::"}
() = \$::{"overload::"};
() = \$::{"utf8::"};
eval { sub { Carp::longmess() }->(\1) };
print $@ eq '' ? "ok 4\n" : "not ok 4\n# $@";

# overload:: glob without hash
undef *{"overload::"};
eval { sub { Carp::longmess() }->(\1) };
print $@ eq '' ? "ok 5\n" : "not ok 5\n# $@";

1;
