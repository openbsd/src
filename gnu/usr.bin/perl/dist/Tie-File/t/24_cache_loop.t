#!/usr/bin/perl
#
# Tests for various caching errors
#

use Config;
my $file = "tf$$.txt";
unless ($Config{d_alarm}) {
  print "1..0\n"; exit;
}

$: = Tie::File::_default_recsep();
my $data = join $:, "record0" .. "record9", "";
my $V = $ENV{INTEGRITY};        # Verbose integrity checking?

print "1..3\n";

my $N = 1;
use Tie::File;
print "ok $N\n"; $N++;

open F, "> $file" or die $!;
binmode F;
print F $data;
close F;

# Limit cache size to 30 bytes 
my $MAX = 30;
#  -- that's enough space for 3 records, but not 4, on both \n and \r\n systems
my $o = tie @a, 'Tie::File', $file, memory => $MAX, autodefer => 1;
print $o ? "ok $N\n" : "not ok $N\n";
$N++;

# (3) In 0.50 this goes into an infinite loop.  Explanation:
#
#   Suppose you overfill the defer buffer by so much that the memory
#   limit is also exceeded.  You'll go into _splice to prepare to
#   write out the defer buffer, and _splice will call _fetch, which
#   will then try to flush the read cache---but the read cache is
#   already empty, so you're stuck in an infinite loop.
#
# Five seconds should be plenty of time for it to complete if it works.
alarm 5 unless $^P;
@a = "record0" .. "record9";
print "ok 3\n";
alarm 0;

END {
  undef $o;
  untie @a;
  1 while unlink $file;
}



