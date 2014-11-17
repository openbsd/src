#!./perl

BEGIN {
    unless (-d 'blib') {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	keys %Config; # Silence warning
	if ($Config{extensions} !~ /\bList\/Util\b/) {
	    print "1..0 # Skip: List::Util was not built\n";
	    exit 0;
	}
    }
}

use Test::More tests => 13;

use List::Util qw(product);

my $v = product;
is( $v, 1, 'no args');

$v = product(9);
is( $v, 9, 'one arg');

$v = product(1,2,3,4);
is( $v, 24, '4 args');

$v = product(-1);
is( $v, -1, 'one -1');

my $x = -3;

$v = product($x, 3);
is( $v, -9, 'variable arg');

$v = product(-3.5,3);
is( $v, -10.5, 'real numbers');

my $one  = Foo->new(1);
my $two  = Foo->new(2);
my $four = Foo->new(4);

$v = product($one,$two,$four);
is($v, 8, 'overload');


{ package Foo;

use overload
  '""' => sub { ${$_[0]} },
  '0+' => sub { ${$_[0]} },
  fallback => 1;
  sub new {
    my $class = shift;
    my $value = shift;
    bless \$value, $class;
  }
}

use Math::BigInt;
my $v1 = Math::BigInt->new(2) ** Math::BigInt->new(65);
my $v2 = $v1 - 1;
$v = product($v1,$v2);
is($v, $v1 * $v2, 'bigint');

$v = product(42, $v1);
is($v, $v1 * 42, 'bigint + builtin int');

$v = product(42, $v1, 2);
is($v, $v1 * 42 * 2, 'bigint + builtin int');

{ package example;

  use overload
    '0+' => sub { $_[0][0] },
    '""' => sub { my $r = "$_[0][0]"; $r = "+$r" unless $r =~ m/^\-/; $r .= " [$_[0][1]]"; $r },
    fallback => 1;

  sub new {
    my $class = shift;

    my $this = bless [@_], $class;

    return $this;
  }
}

{
  my $e1 = example->new(7, "test");
  $t = product($e1, 7, 7);
  is($t, 343, 'overload returning non-overload');
  $t = product(8, $e1, 8);
  is($t, 448, 'overload returning non-overload');
  $t = product(9, 9, $e1);
  is($t, 567, 'overload returning non-overload');
}

