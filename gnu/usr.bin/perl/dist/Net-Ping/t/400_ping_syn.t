use strict;

BEGIN {
  if ($ENV{PERL_CORE}) {
    unless ($ENV{PERL_TEST_Net_Ping}) {
      print "1..0 # Skip: network dependent test\n";
        exit;
    }
  }
  unless (eval "require Socket") {
    print "1..0 \# Skip: no Socket\n";
    exit;
  }
  unless (getservbyname('echo', 'tcp')) {
    print "1..0 \# Skip: no echo port\n";
    exit;
  }
  unless (getservbyname('http', 'tcp')) {
    print "1..0 \# Skip: no http port\n";
    exit;
  }
}

# Remote network test using syn protocol.
#
# NOTE:
#   Network connectivity will be required for all tests to pass.
#   Firewalls may also cause some tests to fail, so test it
#   on a clear network.  If you know you do not have a direct
#   connection to remote networks, but you still want the tests
#   to pass, use the following:
#
# $ PERL_CORE=1 make test

# Try a few remote servers
my %webs;
BEGIN {
  %webs = (
  # Hopefully this is never a routeable host
  "172.29.249.249" => 0,

  # Hopefully all these web ports are open
  "www.geocities.com." => 1,
  "www.freeservers.com." => 1,
  "yahoo.com." => 1,
  "www.yahoo.com." => 1,
  "www.about.com." => 1,
  "www.microsoft.com." => 1,
);
}

use Test::More tests => 3 + 2 * keys %webs;

BEGIN {use_ok('Net::Ping')};

my $can_alarm = eval {alarm 0; 1;};

sub Alarm {
  alarm(shift) if $can_alarm;
}

Alarm(50);
$SIG{ALRM} = sub {
  fail('Alarm timed out');
  die "TIMED OUT!";
};

my $p = new Net::Ping "syn", 10;

isa_ok($p, 'Net::Ping', 'new() worked');

# Change to use the more common web port.
# (Make sure getservbyname works in scalar context.)
cmp_ok(($p->{port_num} = getservbyname("http", "tcp")), '>', 0, 'valid port');

foreach my $host (keys %webs) {
  # ping() does dns resolution and
  # only sends the SYN at this point
  Alarm(50); # (Plenty for a DNS lookup)
  is($p->ping($host), 1, "Can reach $host [" . ($p->{bad}->{$host} || "") . "]");
}

Alarm(20);
while (my $host = $p->ack()) {
  is($webs{$host}, 1, "supposed to be up: http://$host/");
  delete $webs{$host};
}

Alarm(0);
foreach my $host (keys %webs) {
  is($webs{$host}, 0, "supposed to be down: http://$host/ [" . ($p->{bad}->{$host} || "") . "]");
}
