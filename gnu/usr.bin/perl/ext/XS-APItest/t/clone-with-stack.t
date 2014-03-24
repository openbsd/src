#!perl

use strict;
use warnings;

require "../../t/test.pl";

use XS::APItest;

# clone_with_stack creates a clone of the perl interpreter including
# the stack, then destroys the original interpreter and runs the
# remaining code using the new one.
# This is like doing a psuedo-fork and exiting the parent.

use Config;
if (not $Config{'useithreads'}) {
    skip_all("clone_with_stack requires threads");
}

plan(4);

fresh_perl_is( <<'----', <<'====', undef, "minimal clone_with_stack" );
use XS::APItest;
clone_with_stack();
print "ok\n";
----
ok
====

fresh_perl_is( <<'----', <<'====', undef, "inside a subroutine" );
use XS::APItest;
sub f {
    clone_with_stack();
}
f();
print "ok\n";
----
ok
====

{
    local our $TODO = "clone_with_stack inside a begin block";
    fresh_perl_is( <<'----', <<'====', undef, "inside a BEGIN block" );
use XS::APItest;
BEGIN {
    clone_with_stack();
}
print "ok\n";
----
ok
====

}

{
    fresh_perl_is( <<'----', <<'====', undef, "clone stack" );
use XS::APItest;
sub f {
    clone_with_stack();
    0..4;
}
print 'X-', 'Y-', join(':', f()), "-Z\n";
----
X-Y-0:1:2:3:4-Z
====

}
