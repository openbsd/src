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

BEGIN { $| = 1; print "1..25\n"; }

END {print "not ok 1\n" unless $loaded;}

use Time::HiRes qw(tv_interval);

$loaded = 1;

print "ok 1\n";

use strict;

my $have_gettimeofday	= defined &Time::HiRes::gettimeofday;
my $have_usleep		= defined &Time::HiRes::usleep;
my $have_ualarm		= defined &Time::HiRes::ualarm;
my $have_time		= defined &Time::HiRes::time;

import Time::HiRes 'gettimeofday'	if $have_gettimeofday;
import Time::HiRes 'usleep'		if $have_usleep;
import Time::HiRes 'ualarm'		if $have_ualarm;

use Config;

my $have_alarm = $Config{d_alarm};
my $have_fork  = $Config{d_fork};
my $waitfor = 60; # 10 seconds is normal.
my $pid;

if ($have_fork) {
    print "# Testing process $$\n";
    print "# Starting the timer process\n";
    if (defined ($pid = fork())) {
	if ($pid == 0) { # We are the kid, set up the timer.
	    print "# Timer process $$\n";
	    sleep($waitfor);
	    warn "\n$0: overall time allowed for tests (${waitfor}s) exceeded\n";
	    print "# Terminating the testing process\n";
	    kill('TERM', getppid());
	    print "# Timer process exiting\n";
	    exit(0);
	}
    } else {
	warn "$0: fork failed: $!\n";
    }
} else {
    print "# No timer process\n";
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
# Try 20%.  
# Another possibility might be to print "ok" if the test completes fine
# with (say) 10% slosh, "skip - system may have been busy?" if the test
# completes fine with (say) 30% slosh, and fail otherwise.  If you do that,
# consider changing over to test.pl at the same time.
# --A.D., Nov 27, 2001
my $limit = 0.20; # 20% is acceptable slosh for testing timers

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

if (!$have_gettimeofday) {
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

if (!$have_usleep) {
    skip 7..8;
}
else {
    my $one = time;
    usleep(10_000);
    my $two = time;
    usleep(10_000);
    my $three = time;
    ok 7, $one == $two || $two == $three, "slept too long, $one $two $three";

    if (!$have_gettimeofday) {
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

if (!$have_gettimeofday) {
    skip 10;
}
else {
    my $r = [gettimeofday()];
    my $f = tv_interval $r;
    ok 10, $f < 2, $f;
}

if (!$have_usleep || !$have_gettimeofday) {
    skip 11;
}
else {
    my $r = [gettimeofday()];
    Time::HiRes::sleep( 0.5 );
    my $f = tv_interval $r;
    ok 11, $f > 0.4 && $f < 0.9, "slept $f instead of 0.5 secs.";
}

if (!$have_ualarm || !$have_alarm) {
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

if (!$have_time) {
    skip 14;
} else {
 my ($s, $n, $i) = (0);
 for $i (1 .. 100) {
     $s += Time::HiRes::time() - time();
     $n++;
 }
 # $s should be, at worst, equal to $n
 # (time() may be rounding down, up, or closest)
 ok 14, abs($s) / $n <= 1.0, "Time::HiRes::time() not close to time()";
 print "# s = $s, n = $n, s/n = ", $s/$n, "\n";
}

my $has_ualarm = $Config{d_ualarm};

$has_ualarm ||= $xdefine =~ /-DHAS_UALARM/;

unless (   defined &Time::HiRes::gettimeofday
	&& defined &Time::HiRes::ualarm
	&& defined &Time::HiRes::usleep
	&& $has_ualarm) {
    for (15..17) {
	print "ok $_ # Skip: no gettimeofday or no ualarm or no usleep\n";
    }
} else {
    use Time::HiRes qw (time alarm sleep);

    my ($f, $r, $i, $not, $ok);

    $f = time; 
    print "# time...$f\n";
    print "ok 15\n";

    $r = [Time::HiRes::gettimeofday()];
    sleep (0.5);
    print "# sleep...", Time::HiRes::tv_interval($r), "\nok 16\n";

    $r = [Time::HiRes::gettimeofday()];
    $i = 5;
    $SIG{ALRM} = "tick";
    while ($i > 0)
    {
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
	# This test is more sensitive, so impose a softer limit.
	if (abs($ival/$exp - 1) > 3*$limit) {
	    my $ratio = abs($ival/$exp);
	    $not = "while: $exp sleep took $ival ratio $ratio";
	    last;
	}
	$ok = $i;
    }

    sub tick
    {
	$i--;
	my $ival = Time::HiRes::tv_interval ($r);
	print "# Tick! $i $ival\n";
	my $exp = 0.3 * (5 - $i);
	# This test is more sensitive, so impose a softer limit.
	if (abs($ival/$exp - 1) > 3*$limit) {
	    my $ratio = abs($ival/$exp);
	    $not = "tick: $exp sleep took $ival ratio $ratio";
	    $i = 0;
	}
    }

    alarm(0); # can't cancel usig %SIG

    print $not ? "not ok 17 # $not\n" : "ok 17 # $ok\n";
}

unless (   defined &Time::HiRes::setitimer
	&& defined &Time::HiRes::getitimer
	&& eval    'Time::HiRes::ITIMER_VIRTUAL'
	&& $Config{d_select}
	&& $Config{sig_name} =~ m/\bVTALRM\b/) {
    for (18..19) {
	print "ok $_ # Skip: no virtual interval timers\n";
    }
} else {
    use Time::HiRes qw (setitimer getitimer ITIMER_VIRTUAL);

    my $i = 3;
    my $r = [Time::HiRes::gettimeofday()];

    $SIG{VTALRM} = sub {
	$i ? $i-- : setitimer(ITIMER_VIRTUAL, 0);
	print "# Tick! $i ", Time::HiRes::tv_interval($r), "\n";
    };	

    print "# setitimer: ", join(" ", setitimer(ITIMER_VIRTUAL, 0.5, 0.4)), "\n";

    # Assume interval timer granularity of $limit * 0.5 seconds.  Too bold?
    my $virt = getitimer(ITIMER_VIRTUAL);
    print "not " unless defined $virt && abs($virt / 0.5) - 1 < $limit;
    print "ok 18\n";

    print "# getitimer: ", join(" ", getitimer(ITIMER_VIRTUAL)), "\n";

    while (getitimer(ITIMER_VIRTUAL)) {
	my $j;
	for (1..1000) { $j++ } # Can't be unbreakable, must test getitimer().
    }

    print "# getitimer: ", join(" ", getitimer(ITIMER_VIRTUAL)), "\n";

    $virt = getitimer(ITIMER_VIRTUAL);
    print "not " unless defined $virt && $virt == 0;
    print "ok 19\n";

    $SIG{VTALRM} = 'DEFAULT';
}

if ($have_gettimeofday) {
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

eval { sleep(-1) };
print $@ =~ /::sleep\(-1\): negative time not invented yet/ ?
    "ok 22\n" : "not ok 22\n";

eval { usleep(-2) };
print $@ =~ /::usleep\(-2\): negative time not invented yet/ ?
    "ok 23\n" : "not ok 23\n";

if ($have_ualarm) {
    eval { alarm(-3) };
    print $@ =~ /::alarm\(-3, 0\): negative time not invented yet/ ?
	"ok 24\n" : "not ok 24\n";

    eval { ualarm(-4) };
    print $@ =~ /::ualarm\(-4, 0\): negative time not invented yet/ ?
    "ok 25\n" : "not ok 25\n";
} else {
    skip 24;
    skip 25;
}

if (defined $pid) {
    print "# Terminating the timer process $pid\n";
    kill('TERM', $pid); # We are done, the timer can go.
    unlink("ktrace.out");
}

