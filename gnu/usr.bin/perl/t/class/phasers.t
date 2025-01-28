#!./perl

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc('../lib');
    require Config;
}

use v5.36;
use feature 'class';
no warnings 'experimental::class';

# ADJUST
{
    my $adjusted;
    my $class_in_adjust;

    class Testcase1 {
        ADJUST { $adjusted .= "a" }
        ADJUST { $adjusted .= "b" }
        ADJUST { $class_in_adjust = __CLASS__; }
    }

    Testcase1->new;
    is($adjusted, "ab", 'both ADJUST blocks run in order');
    is($class_in_adjust, "Testcase1", 'value of __CLASS__ in ADJUST block');
}

# $self in ADJUST
{
    my $self_in_ADJUST;

    class Testcase2 {
        ADJUST { $self_in_ADJUST = $self; }
    }

    my $obj = Testcase2->new;
    is($self_in_ADJUST, $obj, '$self is set correctly inside ADJUST blocks');
}

done_testing;
