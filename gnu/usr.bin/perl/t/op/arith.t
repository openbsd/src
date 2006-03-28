#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..145\n";

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

my $T = 1;
tryeq $T++,  13 %  4, 1;
tryeq $T++, -13 %  4, 3;
tryeq $T++,  13 % -4, -3;
tryeq $T++, -13 % -4, -1;

# Give abs() a good work-out before using it in anger
tryeq $T++, abs(0), 0;
tryeq $T++, abs(1), 1;
tryeq $T++, abs(-1), 1;
tryeq $T++, abs(2147483647), 2147483647;
tryeq $T++, abs(-2147483647), 2147483647;
tryeq $T++, abs(4294967295), 4294967295;
tryeq $T++, abs(-4294967295), 4294967295;
tryeq $T++, abs(9223372036854775807), 9223372036854775807;
tryeq $T++, abs(-9223372036854775807), 9223372036854775807;
tryeq $T++, abs(1e50), 1e50;	# Assume no change whatever; no slop needed
tryeq $T++, abs(-1e50), 1e50;	# Assume only sign bit flipped

my $limit = 1e6;

# Division (and modulo) of floating point numbers
# seem to be rather sloppy in Cray.
$limit = 1e8 if $^O eq 'unicos';

try $T++, abs( 13e21 %  4e21 -  1e21) < $limit;
try $T++, abs(-13e21 %  4e21 -  3e21) < $limit;
try $T++, abs( 13e21 % -4e21 - -3e21) < $limit;
try $T++, abs(-13e21 % -4e21 - -1e21) < $limit;

# UVs should behave properly

tryeq $T++, 4063328477 % 65535, 27407;
tryeq $T++, 4063328477 % 4063328476, 1;
tryeq $T++, 4063328477 % 2031664238, 1;
tryeq $T++, 2031664238 % 4063328477, 2031664238;

# These should trigger wrapping on 32 bit IVs and UVs

tryeq $T++, 2147483647 + 0, 2147483647;

# IV + IV promote to UV
tryeq $T++, 2147483647 + 1, 2147483648;
tryeq $T++, 2147483640 + 10, 2147483650;
tryeq $T++, 2147483647 + 2147483647, 4294967294;
# IV + UV promote to NV
tryeq $T++, 2147483647 + 2147483649, 4294967296;
# UV + IV promote to NV
tryeq $T++, 4294967294 + 2, 4294967296;
# UV + UV promote to NV
tryeq $T++, 4294967295 + 4294967295, 8589934590;

# UV + IV to IV
tryeq $T++, 2147483648 + -1, 2147483647;
tryeq $T++, 2147483650 + -10, 2147483640;
# IV + UV to IV
tryeq $T++, -1 + 2147483648, 2147483647;
tryeq $T++, -10 + 4294967294, 4294967284;
# IV + IV to NV
tryeq $T++, -2147483648 + -2147483648, -4294967296;
tryeq $T++, -2147483640 + -10, -2147483650;

# Hmm. Don't forget the simple stuff
tryeq $T++, 1 + 1, 2;
tryeq $T++, 4 + -2, 2;
tryeq $T++, -10 + 100, 90;
tryeq $T++, -7 + -9, -16;
tryeq $T++, -63 + +2, -61;
tryeq $T++, 4 + -1, 3;
tryeq $T++, -1 + 1, 0;
tryeq $T++, +29 + -29, 0;
tryeq $T++, -1 + 4, 3;
tryeq $T++, +4 + -17, -13;

# subtraction
tryeq $T++, 3 - 1, 2;
tryeq $T++, 3 - 15, -12;
tryeq $T++, 3 - -7, 10;
tryeq $T++, -156 - 5, -161;
tryeq $T++, -156 - -5, -151;
tryeq $T++, -5 - -12, 7;
tryeq $T++, -3 - -3, 0;
tryeq $T++, 15 - 15, 0;

tryeq $T++, 2147483647 - 0, 2147483647;
tryeq $T++, 2147483648 - 0, 2147483648;
tryeq $T++, -2147483648 - 0, -2147483648;

tryeq $T++, 0 - -2147483647, 2147483647;
tryeq $T++, -1 - -2147483648, 2147483647;
tryeq $T++, 2 - -2147483648, 2147483650;

tryeq $T++, 4294967294 - 3, 4294967291;
tryeq $T++, -2147483648 - -1, -2147483647;

# IV - IV promote to UV
tryeq $T++, 2147483647 - -1, 2147483648;
tryeq $T++, 2147483647 - -2147483648, 4294967295;
# UV - IV promote to NV
tryeq $T++, 4294967294 - -3, 4294967297;
# IV - IV promote to NV
tryeq $T++, -2147483648 - +1, -2147483649;
# UV - UV promote to IV
tryeq $T++, 2147483648 - 2147483650, -2;
# IV - UV promote to IV
tryeq $T++, 2000000000 - 4000000000, -2000000000;

# No warnings should appear;
my $a;
$a += 1;
tryeq $T++, $a, 1;
undef $a;
$a += -1;
tryeq $T++, $a, -1;
undef $a;
$a += 4294967290;
tryeq $T++, $a, 4294967290;
undef $a;
$a += -4294967290;
tryeq $T++, $a, -4294967290;
undef $a;
$a += 4294967297;
tryeq $T++, $a, 4294967297;
undef $a;
$a += -4294967297;
tryeq $T++, $a, -4294967297;

my $s;
$s -= 1;
tryeq $T++, $s, -1;
undef $s;
$s -= -1;
tryeq $T++, $s, +1;
undef $s;
$s -= -4294967290;
tryeq $T++, $s, +4294967290;
undef $s;
$s -= 4294967290;
tryeq $T++, $s, -4294967290;
undef $s;
$s -= 4294967297;
tryeq $T++, $s, -4294967297;
undef $s;
$s -= -4294967297;
tryeq $T++, $s, +4294967297;

# Multiplication

tryeq $T++, 1 * 3, 3;
tryeq $T++, -2 * 3, -6;
tryeq $T++, 3 * -3, -9;
tryeq $T++, -4 * -3, 12;

# check with 0xFFFF and 0xFFFF
tryeq $T++, 65535 * 65535, 4294836225;
tryeq $T++, 65535 * -65535, -4294836225;
tryeq $T++, -65535 * 65535, -4294836225;
tryeq $T++, -65535 * -65535, 4294836225;

# check with 0xFFFF and 0x10001
tryeq $T++, 65535 * 65537, 4294967295;
tryeq $T++, 65535 * -65537, -4294967295;
tryeq $T++, -65535 * 65537, -4294967295;
tryeq $T++, -65535 * -65537, 4294967295;

# check with 0x10001 and 0xFFFF
tryeq $T++, 65537 * 65535, 4294967295;
tryeq $T++, 65537 * -65535, -4294967295;
tryeq $T++, -65537 * 65535, -4294967295;
tryeq $T++, -65537 * -65535, 4294967295;

# These should all be dones as NVs
tryeq $T++, 65537 * 65537, 4295098369;
tryeq $T++, 65537 * -65537, -4295098369;
tryeq $T++, -65537 * 65537, -4295098369;
tryeq $T++, -65537 * -65537, 4295098369;

# will overflow an IV (in 32-bit)
tryeq $T++, 46340 * 46342, 0x80001218;
tryeq $T++, 46340 * -46342, -0x80001218;
tryeq $T++, -46340 * 46342, -0x80001218;
tryeq $T++, -46340 * -46342, 0x80001218;

tryeq $T++, 46342 * 46340, 0x80001218;
tryeq $T++, 46342 * -46340, -0x80001218;
tryeq $T++, -46342 * 46340, -0x80001218;
tryeq $T++, -46342 * -46340, 0x80001218;

# will overflow a positive IV (in 32-bit)
tryeq $T++, 65536 * 32768, 0x80000000;
tryeq $T++, 65536 * -32768, -0x80000000;
tryeq $T++, -65536 * 32768, -0x80000000;
tryeq $T++, -65536 * -32768, 0x80000000;

tryeq $T++, 32768 * 65536, 0x80000000;
tryeq $T++, 32768 * -65536, -0x80000000;
tryeq $T++, -32768 * 65536, -0x80000000;
tryeq $T++, -32768 * -65536, 0x80000000;

# 2147483647 is prime. bah.

tryeq $T++, 46339 * 46341, 0x7ffea80f;
tryeq $T++, 46339 * -46341, -0x7ffea80f;
tryeq $T++, -46339 * 46341, -0x7ffea80f;
tryeq $T++, -46339 * -46341, 0x7ffea80f;

# leading space should be ignored

tryeq $T++, 1 + " 1", 2;
tryeq $T++, 3 + " -1", 2;
tryeq $T++, 1.2, " 1.2";
tryeq $T++, -1.2, " -1.2";

# divide

tryeq $T++, 28/14, 2;
tryeq $T++, 28/-7, -4;
tryeq $T++, -28/4, -7;
tryeq $T++, -28/-2, 14;

tryeq $T++, 0x80000000/1, 0x80000000;
tryeq $T++, 0x80000000/-1, -0x80000000;
tryeq $T++, -0x80000000/1, -0x80000000;
tryeq $T++, -0x80000000/-1, 0x80000000;

# The example for sloppy divide, rigged to avoid the peephole optimiser.
tryeq_sloppy $T++, "20." / "5.", 4;

tryeq $T++, 2.5 / 2, 1.25;
tryeq $T++, 3.5 / -2, -1.75;
tryeq $T++, -4.5 / 2, -2.25;
tryeq $T++, -5.5 / -2, 2.75;

# Bluuurg if your floating point can't accurately cope with powers of 2
# [I suspect this is parsing string->float problems, not actual arith]
tryeq_sloppy $T++, 18446744073709551616/1, 18446744073709551616; # Bluuurg
tryeq_sloppy $T++, 18446744073709551616/2, 9223372036854775808;
tryeq_sloppy $T++, 18446744073709551616/4294967296, 4294967296;
tryeq_sloppy $T++, 18446744073709551616/9223372036854775808, 2;

{
  # The peephole optimiser is wrong to think that it can substitute intops
  # in place of regular ops, because i_multiply can overflow.
  # Bug reported by "Sisyphus" <kalinabears@hdc.com.au>
  my $n = 1127;

  my $float = ($n % 1000) * 167772160.0;
  tryeq_sloppy $T++, $float, 21307064320;

  # On a 32 bit machine, if the i_multiply op is used, you will probably get
  # -167772160. It's actually undefined behaviour, so anything may happen.
  my $int = ($n % 1000) * 167772160;
  tryeq $T++, $int, 21307064320;

  my $t = time;
  my $t1000 = time() * 1000;
  try $T++, abs($t1000 -1000 * $t) <= 2000;
}

my $vms_no_ieee;
if ($^O eq 'VMS') {
  use vars '%Config';
  eval {require Config; import Config};
  $vms_no_ieee = 1 unless defined($Config{useieee});
}

if ($^O eq 'vos') {
  print "not ok ", $T++, " # TODO VOS raises SIGFPE instead of producing infinity.\n";
}
elsif ($vms_no_ieee) {
 print $T++, " # SKIP -- the IEEE infinity model is unavailable in this configuration.\n"
}
elsif ($^O eq 'ultrix') {
  print "not ok ", $T++, " # TODO Ultrix enters deep nirvana instead of producing infinity.\n";
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
  print "ok ", $T++, "\n";
}
