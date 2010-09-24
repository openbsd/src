#!/usr/bin/perl

use lib '..';
use Memoize;
BEGIN {
  eval {require Time::HiRes};
  if ($@ || $ENV{SLOW}) {
#    $SLOW_TESTS = 1;
  } else {
    'Time::HiRes'->import('time');
  }
}

my $DEBUG = 0;

my $n = 0;
$| = 1;

if (-e '.fast') {
  print "1..0\n";
  exit 0;
}

# Perhaps nobody will notice if we don't say anything
# print "# Warning: I'm testing the timed expiration policy.\n# This will take about thirty seconds.\n";

print "1..15\n";
$| = 1;

# (1)
++$n; print "ok $n\n";

# (2)
require Memoize::Expire;
++$n; print "ok $n\n";

sub close_enough {
#  print "Close enough? @_[0,1]\n";
  abs($_[0] - $_[1]) <= 2;
}

sub very_close {
#  print "Close enough? @_[0,1]\n";
  abs($_[0] - $_[1]) <= 0.01;
}

my $t0;
sub start_timer {
  $t0 = time;
  $DEBUG and print "# $t0\n";
}

sub wait_until {
  my $until = shift();
  my $diff = $until - (time() - $t0);
  $DEBUG and print "# until $until; diff = $diff\n";
  return if $diff <= 0;
  select undef, undef, undef, $diff;
}

sub now {
#  print "NOW: @_ ", time(), "\n";
  time;
}

tie my %cache => 'Memoize::Expire', LIFETIME => 15;
memoize 'now',
    SCALAR_CACHE => [HASH => \%cache ],
    LIST_CACHE => 'FAULT'
    ;

# (3)
++$n; print "ok $n\n";


# (4-6)
# T
start_timer();
for (1,2,3) {
  $when{$_} = now($_);
  ++$n;
  print "not " unless close_enough($when{$_}, time());
  print "ok $n\n";
  sleep 6 if $_ < 3;
  $DEBUG and print "# ", time()-$t0, "\n";
}
# values will now expire at T=15, 21, 27
# it is now T=12

# T+12
for (1,2,3) {
  $again{$_} = now($_); # Should be the same as before, because of memoization
}

# (7-9)
# T+12
foreach (1,2,3) {
  ++$n;
  if (very_close($when{$_}, $again{$_})) {
    print "ok $n\n";
  } else {
    print "not ok $n # expected $when{$_}, got $again{$_}\n";
  }
}

# (10)
wait_until(18);  # now(1) expires
print "not " unless close_enough(time, $again{1} = now(1));
++$n; print "ok $n\n";

# (11-12)
# T+18
foreach (2,3) {			# Should not have expired yet.
  ++$n;
  print "not " unless now($_) == $again{$_};
  print "ok $n\n";
}

wait_until(24);  # now(2) expires

# (13)
# T+24
print "not " unless close_enough(time, $again{2} = now(2));
++$n; print "ok $n\n";

# (14-15)
# T+24
foreach (1,3) {  # 1 is good again because it was recomputed after it expired
  ++$n;
  if (very_close(scalar(now($_)), $again{$_})) {
    print "ok $n\n";
  } else {
    print "not ok $n # expected $when{$_}, got $again{$_}\n";
  }
}

