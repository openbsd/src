use strict;
use warnings;

BEGIN {
    if ($ENV{'PERL_CORE'}){
        chdir 't';
        unshift @INC, '../lib';
    }
    use Config;
    if (! $Config{'useithreads'}) {
        print("1..0 # SKIP Perl not compiled with 'useithreads'\n");
        exit(0);
    }
}

use ExtUtils::testlib;

sub ok {
    my ($id, $ok, $name) = @_;

    # You have to do it this way or VMS will get confused.
    if ($ok) {
        print("ok $id - $name\n");
    } else {
        print("not ok $id - $name\n");
        printf("# Failed test at line %d\n", (caller)[2]);
    }

    return ($ok);
}

BEGIN {
    $| = 1;
    print("1..18\n");   ### Number of tests that will be run ###
};

use threads ('stack_size' => 128*4096);
ok(1, 1, 'Loaded');

### Start of Testing ###

ok(2, threads->get_stack_size() == 128*4096,
        'Stack size set in import');
ok(3, threads->set_stack_size(160*4096) == 128*4096,
        'Set returns previous value');
ok(4, threads->get_stack_size() == 160*4096,
        'Get stack size');

threads->create(
    sub {
        ok(5, threads->get_stack_size() == 160*4096,
                'Get stack size in thread');
        ok(6, threads->self()->get_stack_size() == 160*4096,
                'Thread gets own stack size');
        ok(7, threads->set_stack_size(128*4096) == 160*4096,
                'Thread changes stack size');
        ok(8, threads->get_stack_size() == 128*4096,
                'Get stack size in thread');
        ok(9, threads->self()->get_stack_size() == 160*4096,
                'Thread stack size unchanged');
    }
)->join();

ok(10, threads->get_stack_size() == 128*4096,
        'Default thread sized changed in thread');

threads->create(
    { 'stack' => 160*4096 },
    sub {
        ok(11, threads->get_stack_size() == 128*4096,
                'Get stack size in thread');
        ok(12, threads->self()->get_stack_size() == 160*4096,
                'Thread gets own stack size');
    }
)->join();

my $thr = threads->create( { 'stack' => 160*4096 }, sub { } );

$thr->create(
    sub {
        ok(13, threads->get_stack_size() == 128*4096,
                'Get stack size in thread');
        ok(14, threads->self()->get_stack_size() == 160*4096,
                'Thread gets own stack size');
    }
)->join();

$thr->create(
    { 'stack' => 144*4096 },
    sub {
        ok(15, threads->get_stack_size() == 128*4096,
                'Get stack size in thread');
        ok(16, threads->self()->get_stack_size() == 144*4096,
                'Thread gets own stack size');
        ok(17, threads->set_stack_size(160*4096) == 128*4096,
                'Thread changes stack size');
    }
)->join();

$thr->join();

ok(18, threads->get_stack_size() == 160*4096,
        'Default thread sized changed in thread');

exit(0);

# EOF
