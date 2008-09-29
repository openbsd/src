# Test to perform udp protocol testing.

sub isWindowsVista {
   return unless $^O eq 'MSWin32' or $^O eq "cygwin";
   return unless eval { require Win32 };
   return unless defined &Win32::GetOSName;
   return Win32::GetOSName() eq "WinVista";
}

BEGIN {
  unless (eval "require Socket") {
    print "1..0 \# Skip: no Socket\n";
    exit;
  }
  unless (getservbyname('echo', 'udp')) {
    print "1..0 \# Skip: no udp echo port\n";
    exit;
  }

  if(isWindowsVista()) {
    print "1..0 \# Skip: udp ping blocked by Vista's default settings\n";
    exit;
  }
}

use Test;
use Net::Ping;
plan tests => 2;

# Everything loaded fine
ok 1;

my $p = new Net::Ping "udp";
ok $p->ping("127.0.0.1");
