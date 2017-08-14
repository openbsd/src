use strict;

use Test::More 0.82 tests => 2;
use t::Watchdog;

BEGIN { require_ok "Time::HiRes"; }

SKIP: {
    skip "no gettimeofday", 1 unless &Time::HiRes::d_gettimeofday;
    my ($s, $n, $i) = (0);
    for $i (1 .. 100) {
	$s += Time::HiRes::time() - CORE::time();
	$n++;
    }
    # $s should be, at worst, equal to $n
    # (CORE::time() may be rounding down, up, or closest),
    # but allow 10% of slop.
    ok abs($s) / $n <= 1.10
	or note "Time::HiRes::time() not close to CORE::time()";
    note "s = $s, n = $n, s/n = ", abs($s)/$n;
}

1;
