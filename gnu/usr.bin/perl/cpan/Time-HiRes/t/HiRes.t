#!./perl -w

BEGIN {
    if ($ENV{PERL_CORE}) {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	if (" $Config{'extensions'} " !~ m[ Time/HiRes ]) {
	    print "1..0 # Skip -- Perl configured without Time::HiRes module\n";
	    exit 0;
	}
    }
}

BEGIN { $| = 1; print "1..40\n"; }

END { print "not ok 1\n" unless $loaded }

use Time::HiRes 1.9704; # Remember to bump this once in a while.
use Time::HiRes qw(tv_interval);

$loaded = 1;

print "ok 1\n";

use strict;

my $have_gettimeofday	 = &Time::HiRes::d_gettimeofday;
my $have_usleep		 = &Time::HiRes::d_usleep;
my $have_nanosleep	 = &Time::HiRes::d_nanosleep;
my $have_ualarm		 = &Time::HiRes::d_ualarm;
my $have_clock_gettime	 = &Time::HiRes::d_clock_gettime;
my $have_clock_getres	 = &Time::HiRes::d_clock_getres;
my $have_clock_nanosleep = &Time::HiRes::d_clock_nanosleep;
my $have_clock           = &Time::HiRes::d_clock;
my $have_hires_stat      = &Time::HiRes::d_hires_stat;

sub has_symbol {
    my $symbol = shift;
    eval "use Time::HiRes qw($symbol)";
    return 0 unless $@ eq '';
    eval "my \$a = $symbol";
    return $@ eq '';
}

printf "# have_gettimeofday    = %d\n", $have_gettimeofday;
printf "# have_usleep          = %d\n", $have_usleep;
printf "# have_nanosleep       = %d\n", $have_nanosleep;
printf "# have_ualarm          = %d\n", $have_ualarm;
printf "# have_clock_gettime   = %d\n", $have_clock_gettime;
printf "# have_clock_getres    = %d\n", $have_clock_getres;
printf "# have_clock_nanosleep = %d\n", $have_clock_nanosleep;
printf "# have_clock           = %d\n", $have_clock;
printf "# have_hires_stat      = %d\n", $have_hires_stat;

import Time::HiRes 'gettimeofday'	if $have_gettimeofday;
import Time::HiRes 'usleep'		if $have_usleep;
import Time::HiRes 'nanosleep'		if $have_nanosleep;
import Time::HiRes 'ualarm'		if $have_ualarm;
import Time::HiRes 'clock_gettime'	if $have_clock_gettime;
import Time::HiRes 'clock_getres'	if $have_clock_getres;
import Time::HiRes 'clock_nanosleep'	if $have_clock_nanosleep;
import Time::HiRes 'clock'		if $have_clock;

use Config;

use Time::HiRes qw(gettimeofday);

my $have_alarm = $Config{d_alarm};
my $have_fork  = $Config{d_fork};
my $waitfor = 360; # 30-45 seconds is normal (load affects this).
my $timer_pid;
my $TheEnd;

if ($have_fork) {
    print "# I am the main process $$, starting the timer process...\n";
    $timer_pid = fork();
    if (defined $timer_pid) {
	if ($timer_pid == 0) { # We are the kid, set up the timer.
	    my $ppid = getppid();
	    print "# I am the timer process $$, sleeping for $waitfor seconds...\n";
	    sleep($waitfor - 2);    # Workaround for perlbug #49073
	    sleep(2);               # Wait for parent to exit
	    if (kill(0, $ppid)) {   # Check if parent still exists
		warn "\n$0: overall time allowed for tests (${waitfor}s) exceeded!\n";
		print "# Terminating main process $ppid...\n";
		kill('KILL', $ppid);
		print "# This is the timer process $$, over and out.\n";
	    }
	    exit(0);
	} else {
	    print "# The timer process $timer_pid launched, continuing testing...\n";
	    $TheEnd = time() + $waitfor;
	}
    } else {
	warn "$0: fork failed: $!\n";
    }
} else {
    print "# No timer process (need fork)\n";
}

my $xdefine = ''; 

if (open(XDEFINE, "xdefine")) {
    chomp($xdefine = <XDEFINE>);
    close(XDEFINE);
}

# Ideally, we'd like to test that the timers are rather precise.
# However, if the system is busy, there are no guarantees on how
# quickly we will return.  This limit used to be 10%, but that
# was occasionally triggered falsely.  
# So let's try 25%.
# Another possibility might be to print "ok" if the test completes fine
# with (say) 10% slosh, "skip - system may have been busy?" if the test
# completes fine with (say) 30% slosh, and fail otherwise.  If you do that,
# consider changing over to test.pl at the same time.
# --A.D., Nov 27, 2001
my $limit = 0.25; # 25% is acceptable slosh for testing timers

sub skip {
    map { print "ok $_ # skipped\n" } @_;
}

sub ok {
    my ($n, $result, @info) = @_;
    if ($result) {
    	print "ok $n\n";
    }
    else {
	print "not ok $n\n";
    	print "# @info\n" if @info;
    }
}

unless ($have_gettimeofday) {
    skip 2..6;
}
else {
    my @one = gettimeofday();
    ok 2, @one == 2, 'gettimeofday returned ', 0+@one, ' args';
    ok 3, $one[0] > 850_000_000, "@one too small";

    sleep 1;

    my @two = gettimeofday();
    ok 4, ($two[0] > $one[0] || ($two[0] == $one[0] && $two[1] > $one[1])),
    	    "@two is not greater than @one";

    my $f = Time::HiRes::time();
    ok 5, $f > 850_000_000, "$f too small";
    ok 6, $f - $two[0] < 2, "$f - $two[0] >= 2";
}

unless ($have_usleep) {
    skip 7..8;
}
else {
    use Time::HiRes qw(usleep);
    my $one = time;
    usleep(10_000);
    my $two = time;
    usleep(10_000);
    my $three = time;
    ok 7, $one == $two || $two == $three, "slept too long, $one $two $three";

    unless ($have_gettimeofday) {
    	skip 8;
    }
    else {
    	my $f = Time::HiRes::time();
	usleep(500_000);
        my $f2 = Time::HiRes::time();
	my $d = $f2 - $f;
	ok 8, $d > 0.4 && $d < 0.9, "slept $d secs $f to $f2";
    }
}

# Two-arg tv_interval() is always available.
{
    my $f = tv_interval [5, 100_000], [10, 500_000];
    ok 9, abs($f - 5.4) < 0.001, $f;
}

unless ($have_gettimeofday) {
    skip 10;
}
else {
    my $r = [gettimeofday()];
    my $f = tv_interval $r;
    ok 10, $f < 2, $f;
}

unless ($have_usleep && $have_gettimeofday) {
    skip 11;
}
else {
    my $r = [ gettimeofday() ];
    Time::HiRes::sleep( 0.5 );
    my $f = tv_interval $r;
    ok 11, $f > 0.4 && $f < 0.9, "slept $f instead of 0.5 secs.";
}

unless ($have_ualarm && $have_alarm) {
    skip 12..13;
}
else {
    my $tick = 0;
    local $SIG{ ALRM } = sub { $tick++ };

    my $one = time; $tick = 0; ualarm(10_000); while ($tick == 0) { }
    my $two = time; $tick = 0; ualarm(10_000); while ($tick == 0) { }
    my $three = time;
    ok 12, $one == $two || $two == $three, "slept too long, $one $two $three";
    print "# tick = $tick, one = $one, two = $two, three = $three\n";

    $tick = 0; ualarm(10_000, 10_000); while ($tick < 3) { }
    ok 13, 1;
    ualarm(0);
    print "# tick = $tick, one = $one, two = $two, three = $three\n";
}

# Did we even get close?

unless ($have_gettimeofday) {
    skip 14;
} else {
 my ($s, $n, $i) = (0);
 for $i (1 .. 100) {
     $s += Time::HiRes::time() - time();
     $n++;
 }
 # $s should be, at worst, equal to $n
 # (time() may be rounding down, up, or closest),
 # but allow 10% of slop.
 ok 14, abs($s) / $n <= 1.10, "Time::HiRes::time() not close to time()";
 print "# s = $s, n = $n, s/n = ", abs($s)/$n, "\n";
}

my $has_ualarm = $Config{d_ualarm};

$has_ualarm ||= $xdefine =~ /-DHAS_UALARM/;

my $can_subsecond_alarm =
   defined &Time::HiRes::gettimeofday &&
   defined &Time::HiRes::ualarm &&
   defined &Time::HiRes::usleep &&
   $has_ualarm;

unless ($can_subsecond_alarm) {
    for (15..17) {
	print "ok $_ # Skip: no gettimeofday or no ualarm or no usleep\n";
    }
} else {
    use Time::HiRes qw(time alarm sleep);
    eval { require POSIX };
    my $use_sigaction =
	!$@ && defined &POSIX::sigaction && &POSIX::SIGALRM > 0;

    my ($f, $r, $i, $not, $ok);

    $f = time; 
    print "# time...$f\n";
    print "ok 15\n";

    $r = [Time::HiRes::gettimeofday()];
    sleep (0.5);
    print "# sleep...", Time::HiRes::tv_interval($r), "\nok 16\n";

    $r = [Time::HiRes::gettimeofday()];
    $i = 5;
    my $oldaction;
    if ($use_sigaction) {
	$oldaction = new POSIX::SigAction;
	printf "# sigaction tick, ALRM = %d\n", &POSIX::SIGALRM;

	# Perl's deferred signals may be too wimpy to break through
	# a restartable select(), so use POSIX::sigaction if available.

	POSIX::sigaction(&POSIX::SIGALRM,
			 POSIX::SigAction->new("tick"),
			 $oldaction)
	    or die "Error setting SIGALRM handler with sigaction: $!\n";
    } else {
	print "# SIG tick\n";
	$SIG{ALRM} = "tick";
    }

    # On VMS timers can not interrupt select.
    if ($^O eq 'VMS') {
	$ok = "Skip: VMS select() does not get interrupted.";
    } else {
	while ($i > 0) {
	    alarm(0.3);
	    select (undef, undef, undef, 3);
	    my $ival = Time::HiRes::tv_interval ($r);
	    print "# Select returned! $i $ival\n";
	    print "# ", abs($ival/3 - 1), "\n";
	    # Whether select() gets restarted after signals is
	    # implementation dependent.  If it is restarted, we
	    # will get about 3.3 seconds: 3 from the select, 0.3
	    # from the alarm.  If this happens, let's just skip
	    # this particular test.  --jhi
	    if (abs($ival/3.3 - 1) < $limit) {
		$ok = "Skip: your select() may get restarted by your SIGALRM (or just retry test)";
		undef $not;
		last;
	    }
	    my $exp = 0.3 * (5 - $i);
	    if ($exp == 0) {
		$not = "while: divisor became zero";
		last;
	    }
	    # This test is more sensitive, so impose a softer limit.
	    if (abs($ival/$exp - 1) > 4*$limit) {
		my $ratio = abs($ival/$exp);
		$not = "while: $exp sleep took $ival ratio $ratio";
		last;
	    }
	    $ok = $i;
	}
    }

    sub tick {
	$i--;
	my $ival = Time::HiRes::tv_interval ($r);
	print "# Tick! $i $ival\n";
	my $exp = 0.3 * (5 - $i);
	if ($exp == 0) {
	    $not = "tick: divisor became zero";
	    last;
	}
	# This test is more sensitive, so impose a softer limit.
	if (abs($ival/$exp - 1) > 4*$limit) {
	    my $ratio = abs($ival/$exp);
	    $not = "tick: $exp sleep took $ival ratio $ratio";
	    $i = 0;
	}
    }

    if ($use_sigaction) {
	POSIX::sigaction(&POSIX::SIGALRM, $oldaction);
    } else {
	alarm(0); # can't cancel usig %SIG
    }

    print $not ? "not ok 17 # $not\n" : "ok 17 # $ok\n";
}

unless (defined &Time::HiRes::setitimer
	&& defined &Time::HiRes::getitimer
	&& has_symbol('ITIMER_VIRTUAL')
	&& $Config{sig_name} =~ m/\bVTALRM\b/
	&& $^O ne 'nto' # nto: QNX 6 has the API but no implementation
	&& $^O ne 'haiku' # haiku: has the API but no implementation
    ) {
    for (18..19) {
	print "ok $_ # Skip: no virtual interval timers\n";
    }
} else {
    use Time::HiRes qw(setitimer getitimer ITIMER_VIRTUAL);

    my $i = 3;
    my $r = [Time::HiRes::gettimeofday()];

    $SIG{VTALRM} = sub {
	$i ? $i-- : setitimer(&ITIMER_VIRTUAL, 0);
	print "# Tick! $i ", Time::HiRes::tv_interval($r), "\n";
    };	

    print "# setitimer: ", join(" ", setitimer(ITIMER_VIRTUAL, 0.5, 0.4)), "\n";

    # Assume interval timer granularity of $limit * 0.5 seconds.  Too bold?
    my $virt = getitimer(&ITIMER_VIRTUAL);
    print "not " unless defined $virt && abs($virt / 0.5) - 1 < $limit;
    print "ok 18\n";

    print "# getitimer: ", join(" ", getitimer(ITIMER_VIRTUAL)), "\n";

    while (getitimer(&ITIMER_VIRTUAL)) {
	my $j;
	for (1..1000) { $j++ } # Can't be unbreakable, must test getitimer().
    }

    print "# getitimer: ", join(" ", getitimer(ITIMER_VIRTUAL)), "\n";

    $virt = getitimer(&ITIMER_VIRTUAL);
    print "not " unless defined $virt && $virt == 0;
    print "ok 19\n";

    $SIG{VTALRM} = 'DEFAULT';
}

if ($have_gettimeofday &&
    $have_usleep) {
    use Time::HiRes qw(usleep);

    my ($t0, $td);

    my $sleep = 1.5; # seconds
    my $msg;

    $t0 = gettimeofday();
    $a = abs(sleep($sleep)        / $sleep         - 1.0);
    $td = gettimeofday() - $t0;
    my $ratio = 1.0 + $a;

    $msg = "$td went by while sleeping $sleep, ratio $ratio.\n";

    if ($td < $sleep * (1 + $limit)) {
	print $a < $limit ? "ok 20 # $msg" : "not ok 20 # $msg";
    } else {
	print "ok 20 # Skip: $msg";
    }

    $t0 = gettimeofday();
    $a = abs(usleep($sleep * 1E6) / ($sleep * 1E6) - 1.0);
    $td = gettimeofday() - $t0;
    $ratio = 1.0 + $a;

    $msg = "$td went by while sleeping $sleep, ratio $ratio.\n";

    if ($td < $sleep * (1 + $limit)) {
	print $a < $limit ? "ok 21 # $msg" : "not ok 21 # $msg";
    } else {
	print "ok 21 # Skip: $msg";
    }

} else {
    for (20..21) {
	print "ok $_ # Skip: no gettimeofday\n";
    }
}

unless ($have_nanosleep) {
    skip 22..23;
}
else {
    my $one = CORE::time;
    nanosleep(10_000_000);
    my $two = CORE::time;
    nanosleep(10_000_000);
    my $three = CORE::time;
    ok 22, $one == $two || $two == $three, "slept too long, $one $two $three";

    unless ($have_gettimeofday) {
    	skip 23;
    }
    else {
    	my $f = Time::HiRes::time();
	nanosleep(500_000_000);
        my $f2 = Time::HiRes::time();
	my $d = $f2 - $f;
	ok 23, $d > 0.4 && $d < 0.9, "slept $d secs $f to $f2";
    }
}

eval { sleep(-1) };
print $@ =~ /::sleep\(-1\): negative time not invented yet/ ?
    "ok 24\n" : "not ok 24\n";

eval { usleep(-2) };
print $@ =~ /::usleep\(-2\): negative time not invented yet/ ?
    "ok 25\n" : "not ok 25\n";

if ($have_ualarm) {
    eval { alarm(-3) };
    print $@ =~ /::alarm\(-3, 0\): negative time not invented yet/ ?
	"ok 26\n" : "not ok 26\n";

    eval { ualarm(-4) };
    print $@ =~ /::ualarm\(-4, 0\): negative time not invented yet/ ?
    "ok 27\n" : "not ok 27\n";
} else {
    skip 26;
    skip 27;
}

if ($have_nanosleep) {
    eval { nanosleep(-5) };
    print $@ =~ /::nanosleep\(-5\): negative time not invented yet/ ?
	"ok 28\n" : "not ok 28\n";
} else {
    skip 28;
}

# Find the loop size N (a for() loop 0..N-1)
# that will take more than T seconds.

if ($have_ualarm && $] >= 5.008001) {
    # http://groups.google.com/group/perl.perl5.porters/browse_thread/thread/adaffaaf939b042e/20dafc298df737f0%2320dafc298df737f0?sa=X&oi=groupsr&start=0&num=3
    # Perl changes [18765] and [18770], perl bug [perl #20920]

    print "# Finding delay loop...\n";

    my $T = 0.01;
    use Time::HiRes qw(time);
    my $DelayN = 1024;
    my $i;
 N: {
     do {
	 my $t0 = time();
	 for ($i = 0; $i < $DelayN; $i++) { }
	 my $t1 = time();
	 my $dt = $t1 - $t0;
	 print "# N = $DelayN, t1 = $t1, t0 = $t0, dt = $dt\n";
	 last N if $dt > $T;
	 $DelayN *= 2;
     } while (1);
 }

    # The time-burner which takes at least T (default 1) seconds.
    my $Delay = sub {
	my $c = @_ ? shift : 1;
	my $n = $c * $DelayN;
	my $i;
	for ($i = 0; $i < $n; $i++) { }
    };

    # Next setup a periodic timer (the two-argument alarm() of
    # Time::HiRes, behind the curtains the libc getitimer() or
    # ualarm()) which has a signal handler that takes so much time (on
    # the first initial invocation) that the first periodic invocation
    # (second invocation) will happen before the first invocation has
    # finished.  In Perl 5.8.0 the "safe signals" concept was
    # implemented, with unfortunately at least one bug that caused a
    # core dump on reentering the handler. This bug was fixed by the
    # time of Perl 5.8.1.

    # Do not try mixing sleep() and alarm() for testing this.

    my $a = 0; # Number of alarms we receive.
    my $A = 2; # Number of alarms we will handle before disarming.
               # (We may well get $A + 1 alarms.)

    $SIG{ALRM} = sub {
	$a++;
	print "# Alarm $a - ", time(), "\n";
	alarm(0) if $a >= $A; # Disarm the alarm.
	$Delay->(2); # Try burning CPU at least for 2T seconds.
    }; 

    use Time::HiRes qw(alarm); 
    alarm($T, $T);  # Arm the alarm.

    $Delay->(10); # Try burning CPU at least for 10T seconds.

    print "ok 29\n"; # Not core dumping by now is considered to be the success.
} else {
    skip 29;
}

if ($have_clock_gettime &&
    # All implementations of clock_gettime() 
    # are SUPPOSED TO support CLOCK_REALTIME.
    has_symbol('CLOCK_REALTIME')) {
    my $ok = 0;
 TRY: {
	for my $try (1..3) {
	    print "# CLOCK_REALTIME: try = $try\n";
	    my $t0 = clock_gettime(&CLOCK_REALTIME);
	    use Time::HiRes qw(sleep);
	    my $T = 1.5;
	    sleep($T);
	    my $t1 = clock_gettime(&CLOCK_REALTIME);
	    if ($t0 > 0 && $t1 > $t0) {
		print "# t1 = $t1, t0 = $t0\n";
		my $dt = $t1 - $t0;
		my $rt = abs(1 - $dt / $T);
		print "# dt = $dt, rt = $rt\n";
		if ($rt <= 2 * $limit) {
		    $ok = 1;
		    last TRY;
		}
	    } else {
		print "# Error: t0 = $t0, t1 = $t1\n";
	    }
	    my $r = rand() + rand();
	    printf "# Sleeping for %.6f seconds...\n", $r;
	    sleep($r);
	}
    }
    if ($ok) {
	print "ok 30\n";
    } else {
	print "not ok 30\n";
    }
} else {
    print "# No clock_gettime\n";
    skip 30;
}

if ($have_clock_getres) {
    my $tr = clock_getres();
    if ($tr > 0) {
	print "ok 31 # tr = $tr\n";
    } else {
	print "not ok 31 # tr = $tr\n";
    }
} else {
    print "# No clock_getres\n";
    skip 31;
}

if ($have_clock_nanosleep &&
    has_symbol('CLOCK_REALTIME')) {
    my $s = 1.5e9;
    my $t = clock_nanosleep(&CLOCK_REALTIME, $s);
    my $r = abs(1 - $t / $s);
    if ($r < 2 * $limit) {
	print "ok 32\n";
    } else {
	print "not ok 32 # $t = $t, r = $r\n";
    }
} else {
    print "# No clock_nanosleep\n";
    skip 32;
}

if ($have_clock) {
    my @clock = clock();
    print "# clock = @clock\n";
    for my $i (1..3) {
	for (my $j = 0; $j < 1e6; $j++) { }
	push @clock, clock();
	print "# clock = @clock\n";
    }
    if ($clock[0] >= 0 &&
	$clock[1] > $clock[0] &&
	$clock[2] > $clock[1] &&
	$clock[3] > $clock[2]) {
	print "ok 33\n";
    } else {
	print "not ok 33\n";
    }
} else {
    skip 33;
}

sub bellish {  # Cheap emulation of a bell curve.
    my ($min, $max) = @_;
    my $rand = ($max - $min) / 5;
    my $sum = 0; 
    for my $i (0..4) {
	$sum += rand($rand);
    }
    return $min + $sum;
}

if ($have_ualarm) {
    # 1_100_000 sligthly over 1_000_000,
    # 2_200_000 slightly over 2**31/1000,
    # 4_300_000 slightly over 2**32/1000.
    for my $t ([34, 100_000],
	       [35, 1_100_000],
	       [36, 2_200_000],
	       [37, 4_300_000]) {
	my ($i, $n) = @$t;
	my $ok;
	for my $retry (1..10) {
	    my $alarmed = 0;
	    local $SIG{ ALRM } = sub { $alarmed++ };
	    my $t0 = Time::HiRes::time();
	    print "# t0 = $t0\n";
	    print "# ualarm($n)\n";
	    ualarm($n); 1 while $alarmed == 0;
	    my $t1 = Time::HiRes::time();
	    print "# t1 = $t1\n";
	    my $dt = $t1 - $t0;
	    print "# dt = $dt\n";
	    my $r = $dt / ($n/1e6);
	    print "# r = $r\n";
	    $ok =
		($n < 1_000_000 || # Too much noise.
		 ($r >= 0.8 && $r <= 1.6));
	    last if $ok;
	    my $nap = bellish(3, 15);
	    printf "# Retrying in %.1f seconds...\n", $nap;
	    Time::HiRes::sleep($nap);
	}
	ok $i, $ok, "ualarm($n) close enough";
    }
} else {
    print "# No ualarm\n";
    skip 34..37;
}

if ($^O =~ /^(cygwin|MSWin)/) {
    print "# $^O: timestamps may not be good enough\n";
    skip 38;
} elsif (&Time::HiRes::d_hires_stat) {
    my @stat;
    my @atime;
    my @mtime;
    for (1..5) {
	Time::HiRes::sleep(rand(0.1) + 0.1);
	open(X, ">$$");
	print X $$;
	close(X);
	@stat = Time::HiRes::stat($$);
	push @mtime, $stat[9];
	Time::HiRes::sleep(rand(0.1) + 0.1);
	open(X, "<$$");
	<X>;
	close(X);
	@stat = Time::HiRes::stat($$);
	push @atime, $stat[8];
    }
    1 while unlink $$;
    print "# mtime = @mtime\n";
    print "# atime = @atime\n";
    my $ai = 0;
    my $mi = 0;
    my $ss = 0;
    for (my $i = 1; $i < @atime; $i++) {
	if ($atime[$i] >= $atime[$i-1]) {
	    $ai++;
	}
	if ($atime[$i] > int($atime[$i])) {
	    $ss++;
	}
    }
    for (my $i = 1; $i < @mtime; $i++) {
	if ($mtime[$i] >= $mtime[$i-1]) {
	    $mi++;
	}
	if ($mtime[$i] > int($mtime[$i])) {
	    $ss++;
	}
    }
    print "# ai = $ai, mi = $mi, ss = $ss\n";
    # Need at least 75% of monotonical increase and
    # 20% of subsecond results. Yes, this is guessing.
    if ($ss == 0) {
	print "# No subsecond timestamps detected\n";
	skip 38;
    } elsif ($mi/(@mtime-1) >= 0.75 && $ai/(@atime-1) >= 0.75 &&
	     $ss/(@mtime+@atime) >= 0.2) {
	print "ok 38\n";
    } else {
	print "not ok 38\n";
    }
} else {
    print "# No effectual d_hires_stat\n";
    skip 38;
}

unless ($can_subsecond_alarm) {
    skip 39..40;
} else {
    {
	my $alrm;
	$SIG{ALRM} = sub { $alrm++ };
	Time::HiRes::alarm(0.1);
	my $t0 = time();
	1 while time() - $t0 <= 1;
	print $alrm ? "ok 39\n" : "not ok 39\n";
    }
    {
	my $alrm;
	$SIG{ALRM} = sub { $alrm++ };
	Time::HiRes::alarm(1.1);
	my $t0 = time();
	1 while time() - $t0 <= 2;
	print $alrm ? "ok 40\n" : "not ok 40\n";
    }
}

END {
    if ($timer_pid) { # Only in the main process.
	my $left = $TheEnd - time();
	printf "# I am the main process $$, terminating the timer process $timer_pid\n# before it terminates me in %d seconds (testing took %d seconds).\n", $left, $waitfor - $left;
	if (kill(0, $timer_pid)) {
	    local $? = 0;
	    my $kill = kill('KILL', $timer_pid); # We are done, the timer can go.
	    wait();
	    printf "# kill KILL $timer_pid = %d\n", $kill;
	}
	unlink("ktrace.out"); # Used in BSD system call tracing.
	print "# All done.\n";
    }
}

