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

use strict;
use Test::More tests => 8;
use List::Util qw(min);

my $v;

ok(defined &min, 'defined');

$v = min(9);
is($v, 9, 'single arg');

$v = min (1,2);
is($v, 1, '2-arg ordered');

$v = min(2,1);
is($v, 1, '2-arg reverse ordered');

my @a = map { rand() } 1 .. 20;
my @b = sort { $a <=> $b } @a;
$v = min(@a);
is($v, $b[0], '20-arg random order');

my $one = Foo->new(1);
my $two = Foo->new(2);
my $thr = Foo->new(3);

$v = min($one,$two,$thr);
is($v, 1, 'overload');

$v = min($thr,$two,$one);
is($v, 1, 'overload');

{ package Foo;

use overload
  '""' => sub { ${$_[0]} },
  '+0' => sub { ${$_[0]} },
  fallback => 1;
  sub new {
    my $class = shift;
    my $value = shift;
    bless \$value, $class;
  }
}

SKIP: {
  eval { require bignum; } or skip("Need bignum for testing overloading",1);

  my $v1 = 2**65;
  my $v2 = $v1 - 1;
  my $v3 = $v2 - 1;
  $v = min($v1,$v2,$v1,$v3,$v1);
  is($v, $v3, 'bigint');
}
