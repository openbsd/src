use strict;
use warnings;

use Test2::Tools::Basic;
use Test2::API qw/intercept test2_stack context/;
use Test2::Tools::Compare qw/array event end is like/;
use Test2::Tools::Target 'Test2::Plugin::SRand';
use Test2::Tools::Warnings qw/warning/;

test2_stack->top;
my ($root) = test2_stack->all;

sub intercept_2(&) {
    my $code = shift;

    # This is to force loading to happen
    my $ctx = context();

    my @events;

    my $l = $root->listen(sub {
        my ($h, $e) = @_;
        push @events => $e;
    });

    $code->();

    $root->unlisten($l);

    $ctx->release;

    return \@events;
}

{
    local $ENV{HARNESS_IS_VERBOSE} = 1;
    local $ENV{T2_RAND_SEED} = 1234;

    my ($events, $warning);
    my $reseed_qr = qr/SRand loaded multiple times, re-seeding rand/;
    my $reseed_name = "Warned about resetting srand";

    like(
        intercept_2 { $CLASS->import('5555') },
        array {
            event Note => { message => "Seeded srand with seed '5555' from import arg." };
        },
        "got the event"
    );
    is($CLASS->seed, 5555, "set seed");
    is($CLASS->from, 'import arg', "set from");

    $warning = warning { $events = intercept_2 { $CLASS->import(seed => 56789) } };
    like(
        $events,
        array {
            event Note => { message => "Seeded srand with seed '56789' from import arg." };
        },
        "got the event"
    );
    is($CLASS->seed, 56789, "set seed");
    is($CLASS->from, 'import arg', "set from");
    like($warning, $reseed_qr, $reseed_name);

    $warning = warning { $events = intercept_2 { $CLASS->import() } };
    like(
        $events,
        array {
            event Note => { message => "Seeded srand with seed '1234' from environment variable." };
        },
        "got the event"
    );
    is($CLASS->seed, 1234, "set seed");
    is($CLASS->from, 'environment variable', "set from");
    like($warning, $reseed_qr, $reseed_name);

    delete $ENV{T2_RAND_SEED};
    $warning = warning { $events = intercept_2 { $CLASS->import() } };
    like(
        $events,
        array {
            event Note => { message => qr/Seeded srand with seed '\d{8}' from local date\./ };
        },
        "got the event"
    );
    ok($CLASS->seed && $CLASS->seed != 1234, "set seed");
    is($CLASS->from, 'local date', "set from");
    like($warning, $reseed_qr, $reseed_name);

    my $hooks = Test2::API::test2_list_exit_callbacks();
    delete $ENV{HARNESS_IS_VERBOSE};
    $ENV{HARNESS_ACTIVE} = 1;
    warning { $events = intercept { $CLASS->import() } };
    warning { $events = intercept { $CLASS->import() } };
    is(Test2::API::test2_list_exit_callbacks, $hooks + 1, "added hook, but only once");

    warning { $CLASS->import(undef) };
    is($CLASS->seed, 0 , "set seed");
    is($CLASS->from, 'import arg', "set from");
}

done_testing();
