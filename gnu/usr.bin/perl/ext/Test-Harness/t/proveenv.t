#!perl
use strict;
use lib 't/lib';
use Test::More tests => 2;
use App::Prove;

{
    local $ENV{HARNESS_TIMER} = 0;
    my $prv = App::Prove->new;
    ok !$prv->timer, 'timer set via HARNESS_TIMER';
}

{
    local $ENV{HARNESS_TIMER} = 1;
    my $prv = App::Prove->new;
    ok $prv->timer, 'timer set via HARNESS_TIMER';
}
