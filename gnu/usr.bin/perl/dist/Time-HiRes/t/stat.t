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

use Test::More tests => 43;
use t::Watchdog;

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
    ($a, my $lstat, $b) = ("a", [Time::HiRes::lstat($$)], "b");
    is $a, "a";
    is $b, "b";
    is_deeply $lstat, $stat;
    Time::HiRes::sleep(rand(0.1) + 0.1);
    open(X, "<$$");
    <X>;
    close(X);
    $stat = [Time::HiRes::stat($$)];
    push @atime, $stat->[8];
    $lstat = [Time::HiRes::lstat($$)];
    is_deeply $lstat, $stat;
}
1 while unlink $$;
print("# mtime = @mtime\n");
print("# atime = @atime\n");
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
print("# ai = $ai, mi = $mi, ss = $ss\n");
# Need at least 75% of monotonical increase and
# 20% of subsecond results. Yes, this is guessing.
SKIP: {
    skip "no subsecond timestamps detected", 1 if $ss == 0;
    ok $mi/(@mtime-1) >= 0.75 && $ai/(@atime-1) >= 0.75 &&
	     $ss/(@mtime+@atime) >= 0.2;
}

my $targetname = "tgt$$";
my $linkname = "link$$";
SKIP: {
    open(X, ">$targetname");
    print X $$;
    close(X);
    eval { symlink $targetname, $linkname or die "can't symlink: $!"; };
    skip "can't symlink", 7 if $@ ne "";
    my @tgt_stat = Time::HiRes::stat($targetname);
    my @tgt_lstat = Time::HiRes::lstat($targetname);
    my @lnk_stat = Time::HiRes::stat($linkname);
    my @lnk_lstat = Time::HiRes::lstat($linkname);
    is scalar(@tgt_stat), 13;
    is scalar(@tgt_lstat), 13;
    is scalar(@lnk_stat), 13;
    is scalar(@lnk_lstat), 13;
    is_deeply \@tgt_stat, \@tgt_lstat;
    is_deeply \@tgt_stat, \@lnk_stat;
    isnt $lnk_lstat[2], $tgt_stat[2];
}
1 while unlink $linkname;
1 while unlink $targetname;

1;
