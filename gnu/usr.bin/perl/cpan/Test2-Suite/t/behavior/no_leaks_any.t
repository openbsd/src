use Test2::Bundle::Extended;
use Test2::Tools::Spec;
use Test2::Util qw/get_tid/;

my $x;

tests a => {mini => 1, async => 1, iso => 1}, sub { ok(!$x, "a $$ " . get_tid); $x = "a"};
tests b => {mini => 1, async => 1, iso => 1}, sub { ok(!$x, "b $$ " . get_tid); $x = "b"};
tests c => {mini => 1, async => 1, iso => 1}, sub { ok(!$x, "c $$ " . get_tid); $x = "c"};
tests d => {mini => 1, async => 1, iso => 1}, sub { ok(!$x, "d $$ " . get_tid); $x = "d"};
tests e => {mini => 1, async => 1, iso => 1}, sub { ok(!$x, "e $$ " . get_tid); $x = "e"};
tests f => {mini => 1, async => 1, iso => 1}, sub { ok(!$x, "f $$ " . get_tid); $x = "f"};
tests g => {mini => 1, async => 1, iso => 1}, sub { ok(!$x, "g $$ " . get_tid); $x = "g"};

done_testing;

die "Ooops, we leaked |$x|" if $x;
