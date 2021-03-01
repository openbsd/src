#!/usr/bin/perl

use lib '..';
use Memoize;

if (-e '.fast') {
  print "1..0\n";
  exit 0;
}
$| = 1;

# If we don't say anything, maybe nobody will notice.
# print STDERR "\nWarning: I'm testing the speedup.  This might take up to thirty seconds.\n                    ";

my $COARSE_TIME = 1;

sub times_to_time { my ($u) = times; $u; }
if ($^O eq 'riscos') {
  eval {require Time::HiRes; *my_time = \&Time::HiRes::time };
  if ($@) { *my_time = sub { time }; $COARSE_TIME = 1 }
} else {
  *my_time = \&times_to_time;
}


print "1..6\n";



# This next test finds an example that takes a long time to run, then
# checks to make sure that the run is actually speeded up by memoization.
# In some sense, this is the most essential correctness test in the package.  
#
# We do this by running the fib() function with successfily larger
# arguments until we find one that tales at least $LONG_RUN seconds
# to execute.  Then we memoize fib() and run the same call cagain.  If
# it doesn't produce the same test in less than one-tenth the time,
# something is seriously wrong.
#
# $LONG_RUN is the number of seconds that the function call must last
# in order for the call to be considered sufficiently long.


sub fib {
  my $n = shift;
  $COUNT++;
  return $n if $n < 2;
  fib($n-1) + fib($n-2);
}

sub max { $_[0] > $_[1] ? 
          $_[0] : $_[1] 
        }

$N = 1;

$ELAPSED = 0;

my $LONG_RUN = 11;

while (1) {
  my $start = time;
  $COUNT=0;
  $RESULT = fib($N);
  $ELAPSED = time - $start;
  last if $ELAPSED >= $LONG_RUN;
  if ($ELAPSED > 1) {
      print "# fib($N) took $ELAPSED seconds.\n" if $N % 1 == 0;
      # we'd expect that fib(n+1) takes about 1.618 times as long as fib(n)
      # so now that we have a longish run, let's estimate the value of $N
      # that will get us a sufficiently long run.
      $N += 1 + int(log($LONG_RUN/$ELAPSED)/log(1.618));
      print "# OK, N=$N ought to do it.\n";
      # It's important not to overshoot here because the running time
      # is exponential in $N.  If we increase $N too aggressively,
      # the user will be forced to wait a very long time.
  } else {
      $N++; 
  }
}

print "# OK, fib($N) was slow enough; it took $ELAPSED seconds.\n";
print "# Total calls: $COUNT.\n";

&memoize('fib');

$COUNT=0;
$start = time;
$RESULT2 = fib($N);
$ELAPSED2 = time - $start + .001; # prevent division by 0 errors
print (($RESULT == $RESULT2) ? "ok 1\n" : "not ok 1\n");
# If it's not ten times as fast, something is seriously wrong.
print "# ELAPSED2=$ELAPSED2 seconds.\n";
print (($ELAPSED/$ELAPSED2 > 10) ? "ok 2\n" : "not ok 2\n");

# If it called the function more than $N times, it wasn't memoized properly
print (($COUNT > $N) ? "ok 3\n" : "not ok 3\n");

# Do it again. Should be even faster this time.
$COUNT = 0;
$start = time;
$RESULT2 = fib($N);
$ELAPSED2 = time - $start + .001; # prevent division by 0 errors
print (($RESULT == $RESULT2) ? "ok 4\n" : "not ok 4\n");
print "# ELAPSED2=$ELAPSED2 seconds.\n";
print (($ELAPSED/$ELAPSED2 > 10) ? "ok 5\n" : "not ok 5\n");
# This time it shouldn't have called the function at all.
print ($COUNT == 0 ? "ok 6\n" : "not ok 6\n");
