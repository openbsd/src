use strict;
use warnings;

BEGIN {
    use Config;
    if (! $Config{'useithreads'}) {
        print("1..0 # SKIP Perl not compiled with 'useithreads'\n");
        exit(0);
    }
    if ($^O eq 'hpux' && $Config{osvers} <= 10.20) {
        print("1..0 # SKIP Broken under HP-UX 10.20\n");
        exit(0);
    }
}

use ExtUtils::testlib;

BEGIN {
    $| = 1;
    print("1..1\n");   ### Number of tests that will be run ###
};

use threads;
use threads::shared;

### Start of Testing ###

#####
#
# Launches a bunch of threads which are then
# restricted to finishing in numerical order
#
#####
{
    my $cnt = 50;

    my $TIMEOUT = 60;

    my $mutex = 1;
    share($mutex);

    my @threads;
    for (reverse(1..$cnt)) {
        $threads[$_] = threads->create(sub {
                            my $tnum = shift;
                            my $timeout = time() + $TIMEOUT;
                            threads->yield();

                            # Randomize the amount of work the thread does
                            my $sum;
                            for (0..(500000+int(rand(500000)))) {
                                $sum++
                            }

                            # Lock the mutex
                            lock($mutex);

                            # Wait for my turn to finish
                            while ($mutex != $tnum) {
                                if (! cond_timedwait($mutex, $timeout)) {
                                    if ($mutex == $tnum) {
                                        return ('timed out - cond_broadcast not received');
                                    } else {
                                        return ('timed out');
                                    }
                                }
                            }

                            # Finish up
                            $mutex++;
                            cond_broadcast($mutex);
                            return ('okay');
                      }, $_);
    }

    # Gather thread results
    my ($okay, $failures, $timeouts, $unknown) = (0, 0, 0, 0);
    for (1..$cnt) {
        if (! $threads[$_]) {
            $failures++;
        } else {
            my $rc = $threads[$_]->join();
            if (! $rc) {
                $failures++;
            } elsif ($rc =~ /^timed out/) {
                $timeouts++;
            } elsif ($rc eq 'okay') {
                $okay++;
            } else {
                $unknown++;
                print(STDERR "# Unknown error: $rc\n");
            }
        }
    }
    if ($failures) {
        # Most likely due to running out of memory
        print(STDERR "# Warning: $failures threads failed\n");
        print(STDERR "# Note: errno 12 = ENOMEM\n");
        $cnt -= $failures;
    }

    if ($unknown || (($okay + $timeouts) != $cnt)) {
        print("not ok 1\n");
        my $too_few = $cnt - ($okay + $timeouts + $unknown);
        print(STDERR "# Test failed:\n");
        print(STDERR "#\t$too_few too few threads reported\n") if $too_few;
        print(STDERR "#\t$unknown unknown errors\n")           if $unknown;
        print(STDERR "#\t$timeouts threads timed out\n")       if $timeouts;

    } elsif ($timeouts) {
        # Frequently fails under MSWin32 due to deadlocking bug in Windows
        # hence test is TODO under MSWin32
        #   http://rt.perl.org/rt3/Public/Bug/Display.html?id=41574
        #   http://support.microsoft.com/kb/175332
        if ($^O eq 'MSWin32') {
            print("not ok 1 # TODO - not reliable under MSWin32\n")
        } else {
            print("not ok 1\n");
            print(STDERR "# Test failed: $timeouts threads timed out\n");
        }

    } else {
        print("ok 1\n");
    }
}

exit(0);

# EOF
