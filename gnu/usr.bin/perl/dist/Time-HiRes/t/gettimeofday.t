use strict;

BEGIN {
    require Time::HiRes;
    unless(&Time::HiRes::d_gettimeofday) {
	require Test::More;
	Test::More::plan(skip_all => "no gettimeofday()");
    }
}

use Test::More 0.82 tests => 6;
use t::Watchdog;

my @one = Time::HiRes::gettimeofday();
note 'gettimeofday returned ', 0+@one, ' args';
ok @one == 2;
ok $one[0] > 850_000_000 or note "@one too small";

sleep 1;

my @two = Time::HiRes::gettimeofday();
ok $two[0] > $one[0] || ($two[0] == $one[0] && $two[1] > $one[1])
	or note "@two is not greater than @one";

my $f = Time::HiRes::time();
ok $f > 850_000_000 or note "$f too small";
ok $f - $two[0] < 2 or note "$f - $two[0] >= 2";

my $r = [Time::HiRes::gettimeofday()];
my $g = Time::HiRes::tv_interval $r;
ok $g < 2 or note $g;

1;
