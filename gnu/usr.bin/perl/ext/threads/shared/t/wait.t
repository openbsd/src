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
}

use ExtUtils::testlib;

my $Base = 0;
sub ok {
    my ($id, $ok, $name) = @_;
    $id += $Base;

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
    print("1..91\n");   ### Number of tests that will be run ###
};

use threads;
use threads::shared;
ok(1, 1, 'Loaded');
$Base++;

### Start of Testing ###

# cond_wait and cond_timedwait extended tests adapted from cond.t

# The two skips later on in these tests refer to this quote from the
# pod/perl583delta.pod:
#
# =head1 Platform Specific Problems
#
# The regression test ext/threads/shared/t/wait.t fails on early RedHat 9
# and HP-UX 10.20 due to bugs in their threading implementations.
# RedHat users should see https://rhn.redhat.com/errata/RHBA-2003-136.html
# and consider upgrading their glibc.


sub forko (&$$); # To prevent deadlock from underlying pthread_* bugs (as in
                 # stock RH9 glibc/NPTL) or from our own errors, we run tests
                 # in separately forked and alarmed processes.

*forko = ($^O =~ /^dos|os2|mswin32|netware|vms$/i)
? sub (&$$) { my $code = shift; goto &$code; }
: sub (&$$) {
  my ($code, $expected, $patience) = @_;
  my ($test_num, $pid);
  local *CHLD;

  my $bump = $expected;

  unless (defined($pid = open(CHLD, "-|"))) {
    die "fork: $!\n";
  }
  if (! $pid) {   # Child -- run the test
    alarm($patience || 60);
    &$code;
    exit;
  }

  while (<CHLD>) {
    $expected--, $test_num=$1 if /^(?:not )?ok (\d+)/;
    #print "#forko: ($expected, $1) $_";
    print;
  }

  close(CHLD);

  while ($expected--) {
    ok(++$test_num, 0, "missing test result: child status $?");
  }

  $Base += $bump;
};


# - TEST basics

ok(1, defined &cond_wait, "cond_wait() present");
ok(2, (prototype(\&cond_wait) eq '\[$@%];\[$@%]'),
    q|cond_wait() prototype '\[$@%];\[$@%]'|);
ok(3, defined &cond_timedwait, "cond_timedwait() present");
ok(4, (prototype(\&cond_timedwait) eq '\[$@%]$;\[$@%]'),
    q|cond_timedwait() prototype '\[$@%]$;\[$@%]'|);

$Base += 4;

my @wait_how = (
   "simple",  # cond var == lock var; implicit lock; e.g.: cond_wait($c)
   "repeat",  # cond var == lock var; explicit lock; e.g.: cond_wait($c, $c)
   "twain"    # cond var != lock var; explicit lock; e.g.: cond_wait($c, $l)
);

SYNC_SHARED: {
  my $test : shared;  # simple|repeat|twain
  my $cond : shared;
  my $lock : shared;

  ok(1, 1, "Shared synchronization tests preparation");
  $Base += 1;

  sub signaller {
    ok(2,1,"$test: child before lock");
    $test =~ /twain/ ? lock($lock) : lock($cond);
    ok(3,1,"$test: child obtained lock");
    if ($test =~ 'twain') {
      no warnings 'threads';   # lock var != cond var, so disable warnings
      cond_signal($cond);
    } else {
      cond_signal($cond);
    }
    ok(4,1,"$test: child signalled condition");
  }

  # - TEST cond_wait
  forko( sub {
    foreach (@wait_how) {
      $test = "cond_wait [$_]";
      threads->create(\&cw)->join;
      $Base += 5;
    }
  }, 5*@wait_how, 90);

  sub cw {
      # which lock to obtain?
      $test =~ /twain/ ? lock($lock) : lock($cond);
      ok(1,1, "$test: obtained initial lock");

      my $thr = threads->create(\&signaller);
      for ($test) {
        cond_wait($cond), last        if    /simple/;
        cond_wait($cond, $cond), last if    /repeat/;
        cond_wait($cond, $lock), last if    /twain/;
        die "$test: unknown test\n";
      }
      $thr->join;
      ok(5,1, "$test: condition obtained");
  }

  # - TEST cond_timedwait success

  forko( sub {
    foreach (@wait_how) {
      $test = "cond_timedwait [$_]";
      threads->create(\&ctw, 5)->join;
      $Base += 5;
    }
  }, 5*@wait_how, 90);

  sub ctw($) {
      my $to = shift;

      # which lock to obtain?
      $test =~ /twain/ ? lock($lock) : lock($cond);
      ok(1,1, "$test: obtained initial lock");

      my $thr = threads->create(\&signaller);
      my $ok = 0;
      for ($test) {
        $ok=cond_timedwait($cond, time() + $to), last        if    /simple/;
        $ok=cond_timedwait($cond, time() + $to, $cond), last if    /repeat/;
        $ok=cond_timedwait($cond, time() + $to, $lock), last if    /twain/;
        die "$test: unknown test\n";
      }
      $thr->join;
      ok(5,$ok, "$test: condition obtained");
  }

  # - TEST cond_timedwait timeout

  forko( sub {
    foreach (@wait_how) {
      $test = "cond_timedwait pause, timeout [$_]";
      threads->create(\&ctw_fail, 3)->join;
      $Base += 2;
    }
  }, 2*@wait_how, 90);

  forko( sub {
    foreach (@wait_how) {
      $test = "cond_timedwait instant timeout [$_]";
      threads->create(\&ctw_fail, -60)->join;
      $Base += 2;
    }
  }, 2*@wait_how, 90);

  # cond_timedwait timeout (relative timeout)
  sub ctw_fail {
    my $to = shift;
    if ($^O eq "hpux" && $Config{osvers} <= 10.20) {
      # The lock obtaining would pass, but the wait will not.
      ok(1,1, "$test: obtained initial lock");
      ok(2,0, "# SKIP see perl583delta");
    } else {
      $test =~ /twain/ ? lock($lock) : lock($cond);
      ok(1,1, "$test: obtained initial lock");
      my $ok;
      for ($test) {
        $ok=cond_timedwait($cond, time() + $to), last        if    /simple/;
        $ok=cond_timedwait($cond, time() + $to, $cond), last if    /repeat/;
        $ok=cond_timedwait($cond, time() + $to, $lock), last if    /twain/;
        die "$test: unknown test\n";
      }
      ok(2,!defined($ok), "$test: timeout");
    }
  }

} # -- SYNCH_SHARED block


# same as above, but with references to lock and cond vars

SYNCH_REFS: {
  my $test : shared;  # simple|repeat|twain

  my $true_cond; share($true_cond);
  my $true_lock; share($true_lock);

  my $cond = \$true_cond;
  my $lock = \$true_lock;

  ok(1, 1, "Synchronization reference tests preparation");
  $Base += 1;

  sub signaller2 {
    ok(2,1,"$test: child before lock");
    $test =~ /twain/ ? lock($lock) : lock($cond);
    ok(3,1,"$test: child obtained lock");
    if ($test =~ 'twain') {
      no warnings 'threads';   # lock var != cond var, so disable warnings
      cond_signal($cond);
    } else {
      cond_signal($cond);
    }
    ok(4,1,"$test: child signalled condition");
  }

  # - TEST cond_wait
  forko( sub {
    foreach (@wait_how) {
      $test = "cond_wait [$_]";
      threads->create(\&cw2)->join;
      $Base += 5;
    }
  }, 5*@wait_how, 90);

  sub cw2 {
      # which lock to obtain?
      $test =~ /twain/ ? lock($lock) : lock($cond);
      ok(1,1, "$test: obtained initial lock");

      my $thr = threads->create(\&signaller2);
      for ($test) {
        cond_wait($cond), last        if    /simple/;
        cond_wait($cond, $cond), last if    /repeat/;
        cond_wait($cond, $lock), last if    /twain/;
        die "$test: unknown test\n";
      }
      $thr->join;
      ok(5,1, "$test: condition obtained");
  }

  # - TEST cond_timedwait success

  forko( sub {
    foreach (@wait_how) {
      $test = "cond_timedwait [$_]";
      threads->create(\&ctw2, 5)->join;
      $Base += 5;
    }
  }, 5*@wait_how, 90);

  sub ctw2($) {
      my $to = shift;

      # which lock to obtain?
      $test =~ /twain/ ? lock($lock) : lock($cond);
      ok(1,1, "$test: obtained initial lock");

      my $thr = threads->create(\&signaller2);
      my $ok = 0;
      for ($test) {
        $ok=cond_timedwait($cond, time() + $to), last        if    /simple/;
        $ok=cond_timedwait($cond, time() + $to, $cond), last if    /repeat/;
        $ok=cond_timedwait($cond, time() + $to, $lock), last if    /twain/;
        die "$test: unknown test\n";
      }
      $thr->join;
      ok(5,$ok, "$test: condition obtained");
  }

  # - TEST cond_timedwait timeout

  forko( sub {
    foreach (@wait_how) {
      $test = "cond_timedwait pause, timeout [$_]";
      threads->create(\&ctw_fail2, 3)->join;
      $Base += 2;
    }
  }, 2*@wait_how, 90);

  forko( sub {
    foreach (@wait_how) {
      $test = "cond_timedwait instant timeout [$_]";
      threads->create(\&ctw_fail2, -60)->join;
      $Base += 2;
    }
  }, 2*@wait_how, 90);

  sub ctw_fail2 {
    my $to = shift;

    if ($^O eq "hpux" && $Config{osvers} <= 10.20) {
      # The lock obtaining would pass, but the wait will not.
      ok(1,1, "$test: obtained initial lock");
      ok(2,0, "# SKIP see perl583delta");
    } else {
      $test =~ /twain/ ? lock($lock) : lock($cond);
      ok(1,1, "$test: obtained initial lock");
      my $ok;
      for ($test) {
        $ok=cond_timedwait($cond, time() + $to), last        if    /simple/;
        $ok=cond_timedwait($cond, time() + $to, $cond), last if    /repeat/;
        $ok=cond_timedwait($cond, time() + $to, $lock), last if    /twain/;
        die "$test: unknown test\n";
      }
      ok(2,!$ok, "$test: timeout");
    }
  }

} # -- SYNCH_REFS block

# EOF
