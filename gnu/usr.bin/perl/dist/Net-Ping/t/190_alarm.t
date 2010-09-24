# Test to make sure alarm / SIGALM does not interfere
# with Net::Ping.  (This test was derived to ensure
# compatibility with the "spamassassin" utility.)
# Based on code written by radu@netsoft.ro (Radu Greab).

BEGIN {
  if ($ENV{PERL_CORE}) {
    unless ($ENV{PERL_TEST_Net_Ping}) {
      print "1..0 \# Skip: network dependent test\n";
        exit;
    }
  } 
  unless (eval "require Socket") {
    print "1..0 \# Skip: no Socket\n";
    exit;
  }
  unless (eval {alarm 0; 1;}) {
    print "1..0 \# Skip: alarm borks on $^O $^X $] ?\n";
    exit;
  }
  unless (getservbyname('echo', 'tcp')) {
    print "1..0 \# Skip: no echo port\n";
    exit;
  }
}

use strict;
use Test;
use Net::Ping;

plan tests => 6;

# Everything compiled
ok 1;

eval {
  my $timeout = 11;

  ok 1; # In eval
  local $SIG{ALRM} = sub { die "alarm works" };
  ok 1; # SIGALRM can be set on this platform
  alarm $timeout;
  ok 1; # alarm() can be set on this platform

  my $start = time;
  while (1) {
    my $ping = Net::Ping->new("tcp", 2);
    # It does not matter if alive or not
    $ping->ping("127.0.0.1");
    $ping->ping("172.29.249.249");
    die "alarm failed" if time > $start + $timeout + 1;
  }
};
# Got out of "infinite loop" okay
ok 1;

# Make sure it died for a good excuse
ok $@ =~ /alarm works/ or die $@;

alarm 0; # Reset alarm
