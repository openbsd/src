use strict;

use Test::More 0.82 tests => 4;
use t::Watchdog;

BEGIN { require_ok "Time::HiRes"; }

use Config;

my $xdefine = ''; 
if (open(XDEFINE, "xdefine")) {
    chomp($xdefine = <XDEFINE> || "");
    close(XDEFINE);
}

my $can_subsecond_alarm =
   defined &Time::HiRes::gettimeofday &&
   defined &Time::HiRes::ualarm &&
   defined &Time::HiRes::usleep &&
   ($Config{d_ualarm} || $xdefine =~ /-DHAS_UALARM/);

eval { Time::HiRes::sleep(-1) };
like $@, qr/::sleep\(-1\): negative time not invented yet/,
	"negative time error";

SKIP: {
    skip "no subsecond alarm", 2 unless $can_subsecond_alarm;
    my $f = Time::HiRes::time; 
    note "time...$f";
    ok 1;

    my $r = [Time::HiRes::gettimeofday()];
    Time::HiRes::sleep (0.5);
    note "sleep...", Time::HiRes::tv_interval($r);
    ok 1;
}

1;
