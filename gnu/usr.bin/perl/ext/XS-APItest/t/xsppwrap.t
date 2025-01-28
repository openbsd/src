#!perl
#
# Test the XSPP_wrapped() macro.
#
# The XS function set_custom_pp_func() modifies the pp_addr value of the
# op following it to point to a pp function written in the traditional
# non-refcounted style (POPs etc), but which is declared using
# XSPP_wrapped(), so on PERL_RC_STACK builds, the ref count handling of
# the args and return value should still be correct.

use warnings;
use strict;
use Test::More;
use Config;
use XS::APItest qw(set_custom_pp_func);

my ($count, $ret, $c);
sub DESTROY { $count++ }


$count = 0;
{
    my $nine = 9;
    # set_custom_pp_func() overrides the pp func for the next op,
    # which is the pp_add
    ($ret, $c) = (bless(\$nine)+set_custom_pp_func(15), $count);
}

is($ret,  24, "custom add returns correct value");
is($c,     0, "custom add: arg not yet freed");
is($count, 1, "custom add: arg now freed");


$count = 0;
{
    my $nine =    9;
    my $ten  =   10;
    my $eleven = 11;
    # set_custom_pp_func() overrides the pp func for the next op,
    # which is the anonlist
    ($ret, $c) = (
                    [
                        bless(\$nine),
                        bless(\$ten),
                        set_custom_pp_func(bless(\$eleven)),
                    ],
                    $count
                 );
}

ok(defined $ret, "custom anonlist returns defined value");
is(${$ret->[0]},  9, "custom anonlist arg [0]");
is(${$ret->[1]}, 10, "custom anonlist arg [1]");
is(${$ret->[2]}, 11, "custom anonlist arg [2]");
is($c,            0, "custom anonlist args not yet freed");
is($count,        0, "custom anonlist args not yet freed 2");

undef $ret;

is($count,        3, "custom anonlist args now freed");


done_testing();
