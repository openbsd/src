# Test to make sure hires feature works.

BEGIN {
  if ($ENV{PERL_CORE}) {
    unless ($ENV{PERL_TEST_Net_Ping}) {
      print "1..0 # Skip: network dependent test\n";
        exit;
    }
    chdir 't' if -d 't';
    @INC = qw(../lib);
  }
  unless (eval "require Socket") {
    print "1..0 \# Skip: no Socket\n";
    exit;
  }
  unless (eval "require Time::HiRes") {
    print "1..0 \# Skip: no Time::HiRes\n";
    exit;
  }
  unless (getservbyname('echo', 'tcp')) {
    print "1..0 \# Skip: no echo port\n";
    exit;
  }
}

use Test;
use Net::Ping;
plan tests => 8;

# Everything loaded fine
ok 1;

my $p = new Net::Ping "tcp";

# new() worked?
ok !!$p;

# Default is to not use Time::HiRes
ok !$Net::Ping::hires;

# Enable hires
$p -> hires();
ok $Net::Ping::hires;

# Make sure disable works
$p -> hires(0);
ok !$Net::Ping::hires;

# Enable again
$p -> hires(1);
ok $Net::Ping::hires;

# Test on the default port
my ($ret, $duration) = $p -> ping("localhost");

# localhost should always be reachable, right?
ok $ret;

# It is extremely likely that the duration contains a decimal
# point if Time::HiRes is functioning properly.
ok $duration =~ /\./;
