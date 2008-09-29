# Test to make sure object can be instantiated for icmp protocol.
# Root access is required to actually perform icmp testing.

BEGIN {
  unless (eval "require Socket") {
    print "1..0 \# Skip: no Socket\n";
    exit;
  }
}

use Test;
use Net::Ping;
plan tests => 2;

# Everything loaded fine
ok 1;

if (($> and $^O ne 'VMS' and $^O ne 'cygwin')
    or ($^O eq 'MSWin32'
        and !IsAdminUser())
    or ($^O eq 'VMS'
        and (`write sys\$output f\$privilege("SYSPRV")` =~ m/FALSE/))) {
  skip "icmp ping requires root privileges.", 1;
} elsif ($^O eq 'MacOS') {
  skip "icmp protocol not supported.", 1;
} else {
  my $p = new Net::Ping "icmp";
  ok !!$p;
}

sub IsAdminUser {
  return unless $^O eq 'MSWin32';
  return unless eval { require Win32 };
  return unless defined &Win32::IsAdminUser;
  return Win32::IsAdminUser();
}
