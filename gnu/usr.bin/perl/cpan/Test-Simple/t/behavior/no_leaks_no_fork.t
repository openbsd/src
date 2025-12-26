use Test2::Bundle::Extended;
use Test2::Tools::Spec -no_fork => 1;
use Test2::Util qw/get_tid/;

my $x;

tests a => {async => 1, iso => 1}, sub { ok(!$x, "a $$ " . get_tid); $x = "a"};
tests b => {async => 1, iso => 1}, sub { ok(!$x, "b $$ " . get_tid); $x = "b"};
tests c => {async => 1, iso => 1}, sub { ok(!$x, "c $$ " . get_tid); $x = "c"};
tests d => {async => 1, iso => 1}, sub { ok(!$x, "d $$ " . get_tid); $x = "d"};
tests e => {async => 1, iso => 1}, sub { ok(!$x, "e $$ " . get_tid); $x = "e"};
tests f => {async => 1, iso => 1}, sub { ok(!$x, "f $$ " . get_tid); $x = "f"};
tests g => {async => 1, iso => 1}, sub { ok(!$x, "g $$ " . get_tid); $x = "g"};

done_testing;

die "Ooops, we leaked |$x|" if $x;
