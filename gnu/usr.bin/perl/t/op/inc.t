#!./perl -w

# use strict;

print "1..34\n";

my $test = 1;

sub ok {
  my ($pass, $wrong, $err) = @_;
  if ($pass) {
    print "ok $test\n";
    $test = $test + 1; # Would be doubleplusbad to use ++ in the ++ test.
    return 1;
  } else {
    if ($err) {
      chomp $err;
      print "not ok $test # $err\n";
    } else {
      if (defined $wrong) {
        $wrong = ", got $wrong";
      } else {
        $wrong = '';
      }
      printf "not ok $test # line %d$wrong\n", (caller)[2];
    }
  }
  $test = $test + 1;
  return;
}

# Verify that addition/subtraction properly upgrade to doubles.
# These tests are only significant on machines with 32 bit longs,
# and two's complement negation, but shouldn't fail anywhere.

my $a = 2147483647;
my $c=$a++;
ok ($a == 2147483648, $a);

$a = 2147483647;
$c=++$a;
ok ($a == 2147483648, $a);

$a = 2147483647;
$a=$a+1;
ok ($a == 2147483648, $a);

$a = -2147483648;
$c=$a--;
ok ($a == -2147483649, $a);

$a = -2147483648;
$c=--$a;
ok ($a == -2147483649, $a);

$a = -2147483648;
$a=$a-1;
ok ($a == -2147483649, $a);

$a = 2147483648;
$a = -$a;
$c=$a--;
ok ($a == -2147483649, $a);

$a = 2147483648;
$a = -$a;
$c=--$a;
ok ($a == -2147483649, $a);

$a = 2147483648;
$a = -$a;
$a=$a-1;
ok ($a == -2147483649, $a);

$a = 2147483648;
$b = -$a;
$c=$b--;
ok ($b == -$a-1, $a);

$a = 2147483648;
$b = -$a;
$c=--$b;
ok ($b == -$a-1, $a);

$a = 2147483648;
$b = -$a;
$b=$b-1;
ok ($b == -(++$a), $a);

$a = undef;
ok ($a++ eq '0', do { $a=undef; $a++ }, "postinc undef returns '0'");

$a = undef;
ok (!defined($a--), do { $a=undef; $a-- }, "postdec undef returns undef");

# Verify that shared hash keys become unshared.

sub check_same {
  my ($orig, $suspect) = @_;
  my $fail;
  while (my ($key, $value) = each %$suspect) {
    if (exists $orig->{$key}) {
      if ($orig->{$key} ne $value) {
        print "# key '$key' was '$orig->{$key}' now '$value'\n";
        $fail = 1;
      }
    } else {
      print "# key '$key' is '$orig->{$key}', unexpect.\n";
      $fail = 1;
    }
  }
  foreach (keys %$orig) {
    next if (exists $suspect->{$_});
    print "# key '$_' was '$orig->{$_}' now missing\n";
    $fail = 1;
  }
  ok (!$fail);
}

my (%orig) = my (%inc) = my (%dec) = my (%postinc) = my (%postdec)
  = (1 => 1, ab => "ab");
my %up = (1=>2, ab => 'ac');
my %down = (1=>0, ab => -1);

foreach (keys %inc) {
  my $ans = $up{$_};
  my $up;
  eval {$up = ++$_};
  ok ((defined $up and $up eq $ans), $up, $@);
}

check_same (\%orig, \%inc);

foreach (keys %dec) {
  my $ans = $down{$_};
  my $down;
  eval {$down = --$_};
  ok ((defined $down and $down eq $ans), $down, $@);
}

check_same (\%orig, \%dec);

foreach (keys %postinc) {
  my $ans = $postinc{$_};
  my $up;
  eval {$up = $_++};
  ok ((defined $up and $up eq $ans), $up, $@);
}

check_same (\%orig, \%postinc);

foreach (keys %postdec) {
  my $ans = $postdec{$_};
  my $down;
  eval {$down = $_--};
  ok ((defined $down and $down eq $ans), $down, $@);
}

check_same (\%orig, \%postdec);

{
    no warnings 'uninitialized';
    my ($x, $y);
    eval {
	$y ="$x\n";
	++$x;
    };
    ok($x == 1, $x);
    ok($@ eq '', $@);

    my ($p, $q);
    eval {
	$q ="$p\n";
	--$p;
    };
    ok($p == -1, $p);
    ok($@ eq '', $@);
}

$a = 2147483648;
$c=--$a;
ok ($a == 2147483647, $a);


$a = 2147483648;
$c=$a--;
ok ($a == 2147483647, $a);

{
    use integer;
    my $x = 0;
    $x++;
    ok ($x == 1, "(void) i_postinc");
    $x--;
    ok ($x == 0, "(void) i_postdec");
}
