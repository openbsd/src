#!/usr/bin/perl

# test caching timeout

use lib '..';
use Memoize;

my $DEBUG = 0;
my $LIFETIME = 15;

my $test = 0;
$| = 1;

if (-e '.fast') {
  print "1..0\n";
  exit 0;
}

print "# Testing the timed expiration policy.\n";
print "# This will take about thirty seconds.\n";

print "1..26\n";

require Memoize::Expire;
++$test; print "ok $test - Expire loaded\n";

sub now {
#  print "NOW: @_ ", time(), "\n";
  time;
}

tie my %cache => 'Memoize::Expire', LIFETIME => $LIFETIME;

memoize 'now',
    SCALAR_CACHE => [HASH => \%cache ],
    LIST_CACHE => 'FAULT'
    ;

++$test; print "ok $test - function memoized\n";

my (@before, @after, @now);

# Once a second call now(), with three varying indices. Record when
# (within a range) it was called last, and depending on the value returned
# on the next call with the same index, decide whether it correctly
# returned the old value or expired the cache entry.

for my $iteration (0..($LIFETIME/2)) {
    for my $i (0..2) {
        my $before = time;
        my $now = now($i);
        my $after = time;

        # the time returned by now() should either straddle the
        # current time range, or if it returns a cached value, the
        # time range of the previous time it was called.
        # $before..$after represents the time range within which now() must have
        # been called. On very slow platforms, $after - $before may be > 1.

        my $in_range0 = !$iteration || ($before[$i] <= $now && $now <= $after[$i]);
        my $in_range1 = ($before <= $now && $now <= $after);

        my $ok;
        if ($iteration) {
            if ($in_range0) {
                if ($in_range1) {
                    $ok = 0; # this should never happen
                }
                else {
                    # cached value, so cache shouldn't have expired
                    $ok = $after[$i] + $LIFETIME >= $before && $now[$i] == $now;
                }
            }
            else {
                if ($in_range1) {
                    # not cached value, so any cache should have have expired
                    $ok = $before[$i] + $LIFETIME <= $after && $now[$i] != $now;
                }
                else {
                    # not in any range; caching broken
                    $ok = 0;
                }
            }
        }
        else {
            $ok = $in_range1;
        }

        $test++;
        print "not " unless $ok;
        print "ok $test - $iteration:$i\n";
        if (!$ok || $DEBUG) {
            print STDERR sprintf
                "expmod_t.t: %d:%d: r0=%d r1=%d prev=(%s..%s) cur=(%s..%s) now=(%s,%s)\n",
                $iteration, $i, $in_range0, $in_range1,
                $before[$i]||-1, $after[$i]||-1, $before, $after, $now[$i]||-1, $now;
        }

        if (!defined($now[$i]) || $now[$i] != $now) {
            # cache expired; record value of new cache
            $before[$i] = $before;
            $after[$i]  = $after;
            $now[$i]    = $now;
        }

        sleep 1;
    }
}
