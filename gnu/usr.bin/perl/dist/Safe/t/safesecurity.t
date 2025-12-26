#!perl

use strict;
use warnings;
use Config;
use Test::More
    $Config{'extensions'} =~ /\bOpcode\b/
        ? (tests => 1)
        : (skip_all => "no Opcode extension");
use Safe;

my $c = Safe->new;

{
    package My::Controller;
    sub jopa { return "jopa" }
}

$c->reval(q{
    package My::Controller;
    sub jopa { return "hacked" }

    My::Controller->jopa; # let it cache package
});

is(My::Controller->jopa, "jopa", "outside packages cannot be overriden");
