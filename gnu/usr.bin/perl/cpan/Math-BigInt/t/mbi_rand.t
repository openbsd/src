#!/usr/bin/perl -w

use Test::More;
use strict;

my $count;
  
BEGIN
  {
  $| = 1;
  if ($^O eq 'os390') { print "1..0\n"; exit(0) } # test takes too long there
  unshift @INC, '../lib'; # for running manually
  my $location = $0; $location =~ s/mbi_rand.t//;
  unshift @INC, $location; # to locate the testing files
  chdir 't' if -d 't';
  $count = 128;
  plan tests => $count*4;
  }

use Math::BigInt;
my $c = 'Math::BigInt';

my $length = 128;

# If you get a failure here, please re-run the test with the printed seed
# value as input "perl t/mbi_rand.t seed" and send me the output

my $seed = ($#ARGV == 0) ? $ARGV[0] : int(rand(1165537));
print "# seed: $seed\n"; srand($seed);

print "# lib: ", Math::BigInt->config()->{lib},"\n";
if (Math::BigInt->config()->{lib} =~ /::Calc/)
  {
  print "# base len: ", scalar Math::BigInt::Calc->_base_len(),"\n";
  }

my ($A,$B,$As,$Bs,$ADB,$AMB,$la,$lb);
my $two = Math::BigInt->new(2);
for (my $i = 0; $i < $count; $i++)
  {
  # length of A and B
  $la = int(rand($length)+1); $lb = int(rand($length)+1);
  $As = ''; $Bs = '';

  # we create the numbers from "patterns", e.g. get a random number and a
  # random count and string them together. This means things like
  # "100000999999999999911122222222" are much more likely. If we just strung
  # together digits, we would end up with "1272398823211223" etc. It also means
  # that we get more frequently equal numbers or other special cases.
  while (length($As) < $la) { $As .= int(rand(100)) x int(rand(16)); }
  while (length($Bs) < $lb) { $Bs .= int(rand(100)) x int(rand(16)); }

  $As =~ s/^0+//; $Bs =~ s/^0+//; 
  $As = $As || '0'; $Bs = $Bs || '0';
#  print "# As $As\n# Bs $Bs\n";
  $A = $c->new($As); $B = $c->new($Bs);
  print "# A $A\n# B $B\n";
  if ($A->is_zero() || $B->is_zero())
    {
    for (1..4) { is (1,1, 'skipped this test'); } next;
    }

  # check that int(A/B)*B + A % B == A holds for all inputs

  # $X = ($A/$B)*$B + 2 * ($A % $B) - ($A % $B);

  ($ADB,$AMB) = $A->copy()->bdiv($B);
  print "# ($A / $B, $A % $B ) = $ADB $AMB\n";

  print "# seed $seed, ". join(' ',Math::BigInt::Calc->_base_len()),"\n".
        "# tried $ADB * $B + $two*$AMB - $AMB\n"
   unless is ($ADB*$B+$two*$AMB-$AMB,$As, "ADB * B + 2 * AMB - AMB == A");
  if (is ($ADB*$B/$B,$ADB, "ADB * B / B == ADB"))
    {
    print "# seed: $seed, \$ADB * \$B / \$B = ", $ADB * $B / $B, " != $ADB (\$B=$B)\n";
    if (Math::BigInt->config()->{lib} =~ /::Calc/)
      {
      print "# ADB->[-1]: ", $ADB->{value}->[-1], " B->[-1]: ", $B->{value}->[-1],"\n";
      }
    }
  # swap 'em and try this, too
  # $X = ($B/$A)*$A + $B % $A;
  ($ADB,$AMB) = $B->copy()->bdiv($A);
  # print "check: $ADB $AMB";
  print "# seed $seed, ". join(' ',Math::BigInt::Calc->_base_len()),"\n".
        "# tried $ADB * $A + $two*$AMB - $AMB\n"
   unless is ($ADB*$A+$two*$AMB-$AMB,$Bs, "ADB * A + 2 * AMB - AMB == B");
  print "# +$two * $AMB = ",$ADB * $A + $two * $AMB,"\n";
  print "# -$AMB = ",$ADB * $A + $two * $AMB - $AMB,"\n";
  print "# seed $seed, \$ADB * \$A / \$A = ", $ADB * $A / $A, " != $ADB (\$A=$A)\n"
   unless is ($ADB*$A/$A,$ADB, "ADB * A/A == ADB");
  }

