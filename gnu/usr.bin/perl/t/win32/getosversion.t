#!perl -w

# tests for Win32::GetOSVersion()

$^O =~ /^MSWin/ or print("1..0 # not win32\n" ), exit;

print "1..1\n";

my $scalar = Win32::GetOSVersion();
my @array  = Win32::GetOSVersion();

print "not " unless $scalar == $array[4];
print "ok 1\n";
