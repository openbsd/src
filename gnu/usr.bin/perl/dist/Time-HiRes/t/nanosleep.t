use strict;

BEGIN {
    require Time::HiRes;
    unless(&Time::HiRes::d_nanosleep) {
	require Test::More;
	Test::More::plan(skip_all => "no nanosleep()");
    }
}

use Test::More tests => 3;
use t::Watchdog;

eval { Time::HiRes::nanosleep(-5) };
like $@, qr/::nanosleep\(-5\): negative time not invented yet/,
	"negative time error";

my $one = CORE::time;
Time::HiRes::nanosleep(10_000_000);
my $two = CORE::time;
Time::HiRes::nanosleep(10_000_000);
my $three = CORE::time;
ok $one == $two || $two == $three
    or print("# slept too long, $one $two $three\n");

SKIP: {
    skip "no gettimeofday", 1 unless &Time::HiRes::d_gettimeofday;
    my $f = Time::HiRes::time();
    Time::HiRes::nanosleep(500_000_000);
    my $f2 = Time::HiRes::time();
    my $d = $f2 - $f;
    ok $d > 0.4 && $d < 0.9 or print("# slept $d secs $f to $f2\n");
}

1;
