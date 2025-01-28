#!perl
#
# Test the rpp_invoke_xs() function.
# In particular, ensure that an XS CV flagged as CvXS_RCSTACK()
# is called without being wrapped by xs_wrap() but works ok, with no
# leaks or premature frees etc.

use warnings;
use strict;
use Test::More;
use XS::APItest qw(set_xs_rc_stack rc_add);


# Test that $_[0] has the expected refcount

sub rc_is {
    my (undef, $exp_rc, $desc) = @_;
    $exp_rc++ if (Internals::stack_refcounted() & 1);
    is(Internals::SvREFCNT($_[0]), $exp_rc, $desc);
}


# Mark the XS function as 'reference-counted-stack aware'.
# There's no way do do this yet using XS syntax.

set_xs_rc_stack(\&rc_add, 1);


# Basic sanity check

is (rc_add(7,15), 22, "7+15");


# Args with RC==1 should be the same afterwards

{
    my ($x, $y) = (3, 16);
    rc_is($x, 1, '3+16 rc($x) before');
    rc_is($y, 1, '3+16 rc($y) before');
    is (rc_add($x, $y), 19, "3+16");
    rc_is($x, 1, '3+16 rc($x) after');
    rc_is($y, 1, '3+16 rc($y) after');
}


# Return value is a newSV kept alive just by the stack

rc_is(rc_add(34, 17), 1, "rc(34+17)");



done_testing();
