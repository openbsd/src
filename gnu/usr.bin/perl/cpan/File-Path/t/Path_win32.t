use strict;
use Test::More;
use lib 't/';
use FilePathTest;
use File::Path;
use Cwd;
use File::Spec::Functions;

plan skip_all  => 'not win32' unless $^O eq 'MSWin32';
my ($ignore, $major, $minor, $build, $id) = Win32::GetOSVersion();
plan skip_all  => "WinXP or later"
     unless $id >= 2 && ($major > 5 || $major == 5 && $minor >= 1);
plan tests     => 3;

my $tmp_base = catdir(
    curdir(),
    sprintf( 'test-%x-%x-%x', time, $$, rand(99999) ),
);

my $UNC_path = catdir(getcwd(), $tmp_base, 'uncdir');
#dont compute a SMB path with $ENV{COMPUTERNAME}, since SMB may be turned off
#firewalled, disabled, blocked, or no NICs are on and there the PC has no
#working TCPIP stack, \\?\ will always work
$UNC_path = '\\\\?\\'.$UNC_path;

is(mkpath($UNC_path), 2, 'mkpath on Win32 UNC path returns made 2 dir - base and uncdir');

ok(-d $UNC_path, 'mkpath on Win32 UNC path made dir');

my $removed = rmtree($UNC_path);

cmp_ok($removed, '>', 0, "removed $removed entries from $UNC_path");
