#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Config;

print "1..134\n";

sub try ($$) {
   print +($_[1] ? "ok" : "not ok"), " $_[0]\n";
}
sub tryeq ($$$) {
  if ($_[1] == $_[2]) {
    print "ok $_[0]\n";
  } else {
    print "not ok $_[0] # $_[1] != $_[2]\n";
  }
}
sub tryeq_sloppy ($$$) {
  if ($_[1] == $_[2]) {
    print "ok $_[0]\n";
  } else {
    my $error = abs ($_[1] - $_[2]) / $_[1];
    if ($error < 1e-9) {
      print "ok $_[0] # $_[1] is close to $_[2], \$^O eq $^O\n";
    } else {
      print "not ok $_[0] # $_[1] != $_[2]\n";
    }
  }
}

tryeq 1,  13 %  4, 1;
tryeq 2, -13 %  4, 3;
tryeq 3,  13 % -4, -3;
tryeq 4, -13 % -4, -1;

my $limit = 1e6;

# Division (and modulo) of floating point numbers
# seem to be rather sloppy in Cray.
$limit = 1e8 if $^O eq 'unicos';

try 5, abs( 13e21 %  4e21 -  1e21) < $limit;
try 6, abs(-13e21 %  4e21 -  3e21) < $limit;
try 7, abs( 13e21 % -4e21 - -3e21) < $limit;
try 8, abs(-13e21 % -4e21 - -1e21) < $limit;

# UVs should behave properly

tryeq 9, 4063328477 % 65535, 27407;
tryeq 10, 4063328477 % 4063328476, 1;
tryeq 11, 4063328477 % 2031664238, 1;
tryeq 12, 2031664238 % 4063328477, 2031664238;

# These should trigger wrapping on 32 bit IVs and UVs

tryeq 13, 2147483647 + 0, 2147483647;

# IV + IV promote to UV
tryeq 14, 2147483647 + 1, 2147483648;
tryeq 15, 2147483640 + 10, 2147483650;
tryeq 16, 2147483647 + 2147483647, 4294967294;
# IV + UV promote to NV
tryeq 17, 2147483647 + 2147483649, 4294967296;
# UV + IV promote to NV
tryeq 18, 4294967294 + 2, 4294967296;
# UV + UV promote to NV
tryeq 19, 4294967295 + 4294967295, 8589934590;

# UV + IV to IV
tryeq 20, 2147483648 + -1, 2147483647;
tryeq 21, 2147483650 + -10, 2147483640;
# IV + UV to IV
tryeq 22, -1 + 2147483648, 2147483647;
tryeq 23, -10 + 4294967294, 4294967284;
# IV + IV to NV
tryeq 24, -2147483648 + -2147483648, -4294967296;
tryeq 25, -2147483640 + -10, -2147483650;

# Hmm. Don't forget the simple stuff
tryeq 26, 1 + 1, 2;
tryeq 27, 4 + -2, 2;
tryeq 28, -10 + 100, 90;
tryeq 29, -7 + -9, -16;
tryeq 30, -63 + +2, -61;
tryeq 31, 4 + -1, 3;
tryeq 32, -1 + 1, 0;
tryeq 33, +29 + -29, 0;
tryeq 34, -1 + 4, 3;
tryeq 35, +4 + -17, -13;

# subtraction
tryeq 36, 3 - 1, 2;
tryeq 37, 3 - 15, -12;
tryeq 38, 3 - -7, 10;
tryeq 39, -156 - 5, -161;
tryeq 40, -156 - -5, -151;
tryeq 41, -5 - -12, 7;
tryeq 42, -3 - -3, 0;
tryeq 43, 15 - 15, 0;

tryeq 44, 2147483647 - 0, 2147483647;
tryeq 45, 2147483648 - 0, 2147483648;
tryeq 46, -2147483648 - 0, -2147483648;

tryeq 47, 0 - -2147483647, 2147483647;
tryeq 48, -1 - -2147483648, 2147483647;
tryeq 49, 2 - -2147483648, 2147483650;

tryeq 50, 4294967294 - 3, 4294967291;
tryeq 51, -2147483648 - -1, -2147483647;

# IV - IV promote to UV
tryeq 52, 2147483647 - -1, 2147483648;
tryeq 53, 2147483647 - -2147483648, 4294967295;
# UV - IV promote to NV
tryeq 54, 4294967294 - -3, 4294967297;
# IV - IV promote to NV
tryeq 55, -2147483648 - +1, -2147483649;
# UV - UV promote to IV
tryeq 56, 2147483648 - 2147483650, -2;
# IV - UV promote to IV
tryeq 57, 2000000000 - 4000000000, -2000000000;

# No warnings should appear;
my $a;
$a += 1;
tryeq 58, $a, 1;
undef $a;
$a += -1;
tryeq 59, $a, -1;
undef $a;
$a += 4294967290;
tryeq 60, $a, 4294967290;
undef $a;
$a += -4294967290;
tryeq 61, $a, -4294967290;
undef $a;
$a += 4294967297;
tryeq 62, $a, 4294967297;
undef $a;
$a += -4294967297;
tryeq 63, $a, -4294967297;

my $s;
$s -= 1;
tryeq 64, $s, -1;
undef $s;
$s -= -1;
tryeq 65, $s, +1;
undef $s;
$s -= -4294967290;
tryeq 66, $s, +4294967290;
undef $s;
$s -= 4294967290;
tryeq 67, $s, -4294967290;
undef $s;
$s -= 4294967297;
tryeq 68, $s, -4294967297;
undef $s;
$s -= -4294967297;
tryeq 69, $s, +4294967297;

# Multiplication

tryeq 70, 1 * 3, 3;
tryeq 71, -2 * 3, -6;
tryeq 72, 3 * -3, -9;
tryeq 73, -4 * -3, 12;

# check with 0xFFFF and 0xFFFF
tryeq 74, 65535 * 65535, 4294836225;
tryeq 75, 65535 * -65535, -4294836225;
tryeq 76, -65535 * 65535, -4294836225;
tryeq 77, -65535 * -65535, 4294836225;

# check with 0xFFFF and 0x10001
tryeq 78, 65535 * 65537, 4294967295;
tryeq 79, 65535 * -65537, -4294967295;
tryeq 80, -65535 * 65537, -4294967295;
tryeq 81, -65535 * -65537, 4294967295;

# check with 0x10001 and 0xFFFF
tryeq 82, 65537 * 65535, 4294967295;
tryeq 83, 65537 * -65535, -4294967295;
tryeq 84, -65537 * 65535, -4294967295;
tryeq 85, -65537 * -65535, 4294967295;

# These should all be dones as NVs
tryeq 86, 65537 * 65537, 4295098369;
tryeq 87, 65537 * -65537, -4295098369;
tryeq 88, -65537 * 65537, -4295098369;
tryeq 89, -65537 * -65537, 4295098369;

# will overflow an IV (in 32-bit)
tryeq 90, 46340 * 46342, 0x80001218;
tryeq 91, 46340 * -46342, -0x80001218;
tryeq 92, -46340 * 46342, -0x80001218;
tryeq 93, -46340 * -46342, 0x80001218;

tryeq 94, 46342 * 46340, 0x80001218;
tryeq 95, 46342 * -46340, -0x80001218;
tryeq 96, -46342 * 46340, -0x80001218;
tryeq 97, -46342 * -46340, 0x80001218;

# will overflow a positive IV (in 32-bit)
tryeq 98, 65536 * 32768, 0x80000000;
tryeq 99, 65536 * -32768, -0x80000000;
tryeq 100, -65536 * 32768, -0x80000000;
tryeq 101, -65536 * -32768, 0x80000000;

tryeq 102, 32768 * 65536, 0x80000000;
tryeq 103, 32768 * -65536, -0x80000000;
tryeq 104, -32768 * 65536, -0x80000000;
tryeq 105, -32768 * -65536, 0x80000000;

# 2147483647 is prime. bah.

tryeq 106, 46339 * 46341, 0x7ffea80f;
tryeq 107, 46339 * -46341, -0x7ffea80f;
tryeq 108, -46339 * 46341, -0x7ffea80f;
tryeq 109, -46339 * -46341, 0x7ffea80f;

# leading space should be ignored

tryeq 110, 1 + " 1", 2;
tryeq 111, 3 + " -1", 2;
tryeq 112, 1.2, " 1.2";
tryeq 113, -1.2, " -1.2";

# divide

tryeq 114, 28/14, 2;
tryeq 115, 28/-7, -4;
tryeq 116, -28/4, -7;
tryeq 117, -28/-2, 14;

tryeq 118, 0x80000000/1, 0x80000000;
tryeq 119, 0x80000000/-1, -0x80000000;
tryeq 120, -0x80000000/1, -0x80000000;
tryeq 121, -0x80000000/-1, 0x80000000;

# The example for sloppy divide, rigged to avoid the peephole optimiser.
tryeq_sloppy 122, "20." / "5.", 4;

tryeq 123, 2.5 / 2, 1.25;
tryeq 124, 3.5 / -2, -1.75;
tryeq 125, -4.5 / 2, -2.25;
tryeq 126, -5.5 / -2, 2.75;

# Bluuurg if your floating point can't accurately cope with powers of 2
# [I suspect this is parsing string->float problems, not actual arith]
tryeq_sloppy 127, 18446744073709551616/1, 18446744073709551616; # Bluuurg
tryeq_sloppy 128, 18446744073709551616/2, 9223372036854775808;
tryeq_sloppy 129, 18446744073709551616/4294967296, 4294967296;
tryeq_sloppy 130, 18446744073709551616/9223372036854775808, 2;

{
  # The peephole optimiser is wrong to think that it can substitute intops
  # in place of regular ops, because i_multiply can overflow.
  # Bug reported by "Sisyphus" <kalinabears@hdc.com.au>
  my $n = 1127;

  my $float = ($n % 1000) * 167772160.0;
  tryeq_sloppy 131, $float, 21307064320;

  # On a 32 bit machine, if the i_multiply op is used, you will probably get
  # -167772160. It's actually undefined behaviour, so anything may happen.
  my $int = ($n % 1000) * 167772160;
  tryeq 132, $int, 21307064320;

  my $t = time;
  my $t1000 = time() * 1000;
  try 133, abs($t1000 -1000 * $t) <= 2000;
}

if ($^O eq 'vos') {
  print "not ok 134 # TODO VOS raises SIGFPE instead of producing infinity.\n";
} 
elsif (($^O eq 'VMS') && !defined($Config{useieee})) {
  print "ok 134 # SKIP -- the IEEE infinity model is unavailable in this configuration.\n";
} 
else {
  # The computation of $v should overflow and produce "infinity"
  # on any system whose max exponent is less than 10**1506.
  # The exact string used to represent infinity varies by OS,
  # so we don't test for it; all we care is that we don't die.
  #
  # Perl considers it to be an error if SIGFPE is raised.
  # Chances are the interpreter will die, since it doesn't set
  # up a handler for SIGFPE.  That's why this test is last; to
  # minimize the number of test failures.  --PG

  my $n = 5000;
  my $v = 2;
  while (--$n)
  {
    $v *= 2;
  }
  print "ok 134\n";
}
