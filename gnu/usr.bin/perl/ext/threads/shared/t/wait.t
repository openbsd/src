# cond_wait and cond_timedwait extended tests
# adapted from cond.t

use warnings;

BEGIN {
    chdir 't' if -d 't';
    push @INC ,'../lib';
    require Config; import Config;
    unless ($Config{'useithreads'}) {
        print "1..0 # Skip: no threads\n";
        exit 0;
    }
}
$|++;
print "1..102\n";
use strict;

use threads;
use threads::shared;
use ExtUtils::testlib;

my $Base = 0;

sub ok {
    my ($offset, $bool, $text) = @_;
    my $not = '';
    $not = "not " unless $bool;
    print "${not}ok " . ($Base + $offset) . " - $text\n";
}

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

  $patience ||= 60;

  unless (defined($pid = open(CHLD, "-|"))) {
    die "fork: $!\n";
  }
  if (! $pid) {   # Child -- run the test
    $patience ||= 60;
    alarm $patience;
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
    $test_num++;
    print "not ok $test_num - child status $?\n";
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

  print "# testing my \$var : shared\n";
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
      $Base += 6;
    }
  }, 6*@wait_how, 90);

  sub cw {
    my $thr;

    { # -- begin lock scope; which lock to obtain?
      $test =~ /twain/ ? lock($lock) : lock($cond);
      ok(1,1, "$test: obtained initial lock");

      $thr = threads->create(\&signaller);
      for ($test) {
        cond_wait($cond), last        if    /simple/;
        cond_wait($cond, $cond), last if    /repeat/;
        cond_wait($cond, $lock), last if    /twain/;
        die "$test: unknown test\n"; 
      }
      ok(5,1, "$test: condition obtained");
    } # -- end lock scope

    $thr->join;
    ok(6,1, "$test: join completed");
  }

  # - TEST cond_timedwait success

  forko( sub {
    foreach (@wait_how) {
      $test = "cond_timedwait [$_]";
      threads->create(\&ctw, 5)->join;
      $Base += 6;
    }
  }, 6*@wait_how, 90);

  sub ctw($) {
    my $to = shift;
    my $thr;

    { # -- begin lock scope;  which lock to obtain?
      $test =~ /twain/ ? lock($lock) : lock($cond);
      ok(1,1, "$test: obtained initial lock");

      $thr = threads->create(\&signaller);
      my $ok = 0;
      for ($test) {
        $ok=cond_timedwait($cond, time() + $to), last        if    /simple/;
        $ok=cond_timedwait($cond, time() + $to, $cond), last if    /repeat/;
        $ok=cond_timedwait($cond, time() + $to, $lock), last if    /twain/;
        die "$test: unknown test\n"; 
      }
      ok(5,$ok, "$test: condition obtained");
    } # -- end lock scope

    $thr->join;
    ok(6,1, "$test: join completed");
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

} # -- SYNCH_SHARED block


# same as above, but with references to lock and cond vars

SYNCH_REFS: {
  my $test : shared;  # simple|repeat|twain
  
  my $true_cond; share($true_cond);
  my $true_lock; share($true_lock);

  my $cond = \$true_cond;
  my $lock = \$true_lock;

  print "# testing reference to shared(\$var)\n";
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
      $Base += 6;
    }
  }, 6*@wait_how, 90);

  sub cw2 {
    my $thr;

    { # -- begin lock scope; which lock to obtain?
      $test =~ /twain/ ? lock($lock) : lock($cond);
      ok(1,1, "$test: obtained initial lock");

      $thr = threads->create(\&signaller2);
      for ($test) {
        cond_wait($cond), last        if    /simple/;
        cond_wait($cond, $cond), last if    /repeat/;
        cond_wait($cond, $lock), last if    /twain/;
        die "$test: unknown test\n"; 
      }
      ok(5,1, "$test: condition obtained");
    } # -- end lock scope

    $thr->join;
    ok(6,1, "$test: join completed");
  }

  # - TEST cond_timedwait success

  forko( sub {
    foreach (@wait_how) {
      $test = "cond_timedwait [$_]";
      threads->create(\&ctw2, 5)->join;
      $Base += 6;
    }
  }, 6*@wait_how, 90);

  sub ctw2($) {
    my $to = shift;
    my $thr;

    { # -- begin lock scope;  which lock to obtain?
      $test =~ /twain/ ? lock($lock) : lock($cond);
      ok(1,1, "$test: obtained initial lock");

      $thr = threads->create(\&signaller2);
      my $ok = 0;
      for ($test) {
        $ok=cond_timedwait($cond, time() + $to), last        if    /simple/;
        $ok=cond_timedwait($cond, time() + $to, $cond), last if    /repeat/;
        $ok=cond_timedwait($cond, time() + $to, $lock), last if    /twain/;
        die "$test: unknown test\n"; 
      }
      ok(5,$ok, "$test: condition obtained");
    } # -- end lock scope

    $thr->join;
    ok(6,1, "$test: join completed");
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

} # -- SYNCH_REFS block

