use strict;

BEGIN {
    require Time::HiRes;
    unless(&Time::HiRes::d_hires_stat) {
	require Test::More;
	Test::More::plan(skip_all => "no hi-res stat");
    }
    if($^O =~ /\A(?:cygwin|MSWin)/) {
	require Test::More;
	Test::More::plan(skip_all =>
		"$^O file timestamps not reliable enough for stat test");
    }
}

use Test::More 0.82 tests => 16;
use t::Watchdog;

my $limit = 0.25; # 25% is acceptable slosh for testing timers

my @atime;
my @mtime;
for (1..5) {
    Time::HiRes::sleep(rand(0.1) + 0.1);
    open(X, ">$$");
    print X $$;
    close(X);
    my($a, $stat, $b) = ("a", [Time::HiRes::stat($$)], "b");
    is $a, "a";
    is $b, "b";
    is ref($stat), "ARRAY";
    push @mtime, $stat->[9];
    Time::HiRes::sleep(rand(0.1) + 0.1);
    open(X, "<$$");
    <X>;
    close(X);
    $stat = [Time::HiRes::stat($$)];
    push @atime, $stat->[8];
}
1 while unlink $$;
note "mtime = @mtime";
note "atime = @atime";
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
note "ai = $ai, mi = $mi, ss = $ss";
# Need at least 75% of monotonical increase and
# 20% of subsecond results. Yes, this is guessing.
SKIP: {
    skip "no subsecond timestamps detected", 1 if $ss == 0;
    ok $mi/(@mtime-1) >= 0.75 && $ai/(@atime-1) >= 0.75 &&
	     $ss/(@mtime+@atime) >= 0.2;
}

1;
