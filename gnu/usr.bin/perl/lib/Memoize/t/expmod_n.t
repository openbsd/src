#!/usr/bin/perl

use lib '..';
use Memoize;

my $n = 0;


print "1..22\n";

++$n; print "ok $n\n";

$RETURN = 1;

%CALLS = ();
sub call {
#  print "CALL $_[0] => $RETURN\n";
  ++$CALLS{$_[0]};
  $RETURN;
}

require Memoize::Expire;
++$n; print "ok $n\n";

tie my %cache => 'Memoize::Expire', NUM_USES => 2;
memoize 'call',
    SCALAR_CACHE => [HASH => \%cache],
    LIST_CACHE => 'FAULT';

# $Memoize::Expire::DEBUG = 1;
++$n; print "ok $n\n";

# 3--6
for (0,1,2,3) {
  print "not " unless call($_) == 1;
  ++$n; print "ok $n\n";
}

# 7--10
for (keys %CALLS) {
  print "not " unless $CALLS{$_} == (1,1,1,1)[$_];
  ++$n; print "ok $n\n";
}

# 11--13
$RETURN = 2;
++$n; print ((call(1) == 1 ? '' : 'not '), "ok $n\n"); # 1 expires
++$n; print ((call(1) == 2 ? '' : 'not '), "ok $n\n"); # 1 gets new val
++$n; print ((call(2) == 1 ? '' : 'not '), "ok $n\n"); # 2 expires

# 14--17
$RETURN = 3;
for (0,1,2,3) {
  # 0 expires, 1 expires, 2 gets new val, 3 expires
  print "not " unless call($_) == (1,2,3,1)[$_];
  ++$n; print "ok $n\n";
}

for (0,1,2,3) {
  print "not " unless $CALLS{$_} == (1,2,2,1)[$_];
  ++$n; print "ok $n\n";
}
