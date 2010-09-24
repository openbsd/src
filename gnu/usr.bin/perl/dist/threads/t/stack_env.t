use strict;
use warnings;

BEGIN {
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
    print("1..4\n");   ### Number of tests that will be run ###

    $ENV{'PERL5_ITHREADS_STACK_SIZE'} = 128*4096;
};

use threads;
ok(1, 1, 'Loaded');

### Start of Testing ###

ok(2, threads->get_stack_size() == 128*4096,
        '$ENV{PERL5_ITHREADS_STACK_SIZE}');
ok(3, threads->set_stack_size(144*4096) == 128*4096,
        'Set returns previous value');
ok(4, threads->get_stack_size() == 144*4096,
        'Get stack size');

exit(0);

# EOF
