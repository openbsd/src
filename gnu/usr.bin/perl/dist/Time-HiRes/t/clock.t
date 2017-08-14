use strict;

use Test::More 0.82 tests => 5;
use t::Watchdog;

BEGIN { require_ok "Time::HiRes"; }

sub has_symbol {
    my $symbol = shift;
    eval "use Time::HiRes qw($symbol)";
    return 0 unless $@ eq '';
    eval "my \$a = $symbol";
    return $@ eq '';
}

note sprintf "have_clock_gettime   = %d", &Time::HiRes::d_clock_gettime;
note sprintf "have_clock_getres    = %d", &Time::HiRes::d_clock_getres;
note sprintf "have_clock_nanosleep = %d", &Time::HiRes::d_clock_nanosleep;
note sprintf "have_clock           = %d", &Time::HiRes::d_clock;

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

SKIP: {
    skip "no clock_gettime", 1
	unless &Time::HiRes::d_clock_gettime && has_symbol("CLOCK_REALTIME");
    my $ok = 0;
 TRY: {
	for my $try (1..3) {
	    note "CLOCK_REALTIME: try = $try";
	    my $t0 = Time::HiRes::clock_gettime(&CLOCK_REALTIME);
	    my $T = 1.5;
	    Time::HiRes::sleep($T);
	    my $t1 = Time::HiRes::clock_gettime(&CLOCK_REALTIME);
	    if ($t0 > 0 && $t1 > $t0) {
		note "t1 = $t1, t0 = $t0";
		my $dt = $t1 - $t0;
		my $rt = abs(1 - $dt / $T);
		note "dt = $dt, rt = $rt";
		if ($rt <= 2 * $limit) {
		    $ok = 1;
		    last TRY;
		}
	    } else {
		note "Error: t0 = $t0, t1 = $t1";
	    }
	    my $r = rand() + rand();
	    note sprintf "Sleeping for %.6f seconds...\n", $r;
	    Time::HiRes::sleep($r);
	}
    }
    ok $ok;
}

SKIP: {
    skip "no clock_getres", 1 unless &Time::HiRes::d_clock_getres;
    my $tr = Time::HiRes::clock_getres();
    ok $tr > 0 or note "tr = $tr";
}

SKIP: {
    skip "no clock_nanosleep", 1
	unless &Time::HiRes::d_clock_nanosleep && has_symbol("CLOCK_REALTIME");
    my $s = 1.5e9;
    my $t = Time::HiRes::clock_nanosleep(&CLOCK_REALTIME, $s);
    my $r = abs(1 - $t / $s);
    ok $r < 2 * $limit or note "t = $t, r = $r";
}

SKIP: {
    skip "no clock", 1 unless &Time::HiRes::d_clock;
    my @clock = Time::HiRes::clock();
    note "clock = @clock";
    for my $i (1..3) {
	for (my $j = 0; $j < 1e6; $j++) { }
	push @clock, Time::HiRes::clock();
	note "clock = @clock";
    }
    ok $clock[0] >= 0 &&
	$clock[1] > $clock[0] &&
	$clock[2] > $clock[1] &&
	$clock[3] > $clock[2];
}

1;
