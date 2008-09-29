use strict;
use warnings;

BEGIN {
    if ($ENV{'PERL_CORE'}){
        chdir 't';
        unshift @INC, '../lib';
    }
    use Config;
    if (! $Config{'useithreads'}) {
        print("1..0 # Skip: Perl not compiled with 'useithreads'\n");
        exit(0);
    }
    if ($^O eq 'hpux' && $Config{osvers} <= 10.20) {
        print("1..0 # Skip: Broken under HP-UX 10.20\n");
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

    my $TIMEOUT = 30;

    my $mutex = 1;
    share($mutex);

    my @threads;
    for (1..$cnt) {
        $threads[$_] = threads->create(sub {
                            my $tnum = shift;
                            my $timeout = time() + $TIMEOUT;

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
        my $rc = $threads[$_]->join();
        if (! $rc) {
            $failures++;
        } elsif ($rc =~ /^timed out/) {
            $timeouts++;
        } elsif ($rc eq 'okay') {
            $okay++;
        } else {
            $unknown++;
            print("# Unknown error: $rc\n");
        }
    }

    if ($failures || $unknown || (($okay + $timeouts) != $cnt)) {
        print('not ok 1');
        my $too_few = $cnt - ($okay + $failures + $timeouts + $unknown);
        print(" - $too_few too few threads reported") if $too_few;
        print(" - $failures threads failed")          if $failures;
        print(" - $unknown unknown errors")           if $unknown;
        print(" - $timeouts threads timed out")       if $timeouts;
        print("\n");

    } elsif ($timeouts) {
        # Frequently fails under MSWin32 due to deadlocking bug in Windows
        # hence test is TODO under MSWin32
        #   http://rt.perl.org/rt3/Public/Bug/Display.html?id=41574
        #   http://support.microsoft.com/kb/175332
        print('not ok 1');
        print(' # TODO - not reliable under MSWin32') if ($^O eq 'MSWin32');
        print(" - $timeouts threads timed out\n");

    } else {
        print('ok 1');
        print(' # TODO - not reliable under MSWin32') if ($^O eq 'MSWin32');
        print("\n");
    }
}

# EOF
