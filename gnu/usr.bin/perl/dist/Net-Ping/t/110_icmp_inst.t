# Test to make sure object can be instantiated for icmp protocol.
# Root access is required to actually perform icmp testing.

use strict;

BEGIN {
  unless (eval "require Socket") {
    print "1..0 \# Skip: no Socket\n";
    exit;
  }
}

use Test::More tests => 2;
BEGIN {use_ok('Net::Ping')};

SKIP: {
  skip "icmp ping requires root privileges.", 1
    if ($> and $^O ne 'VMS' and $^O ne 'cygwin')
      or (($^O eq 'MSWin32' or $^O eq 'cygwin')
	  and !IsAdminUser())
	or ($^O eq 'VMS'
	    and (`write sys\$output f\$privilege("SYSPRV")` =~ m/FALSE/));
  my $p = new Net::Ping "icmp";
  isa_ok($p, 'Net::Ping', 'object can be instantiated for icmp protocol');
}

sub IsAdminUser {
  return unless $^O eq 'MSWin32' or $^O eq 'cygwin';
  return unless eval { require Win32 };
  return unless defined &Win32::IsAdminUser;
  return Win32::IsAdminUser();
}
