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


use Test::More tests => 29;

use Scalar::Util qw(refaddr);
use vars qw($t $y $x *F $v $r);
use Symbol qw(gensym);

# Ensure we do not trigger and tied methods
tie *F, 'MyTie';

my $i = 1;
foreach $v (undef, 10, 'string') {
  is(refaddr($v), undef, "not " . (defined($v) ? "'$v'" : "undef"));
}

foreach $r ({}, \$t, [], \*F, sub {}) {
  my $n = "$r";
  $n =~ /0x(\w+)/;
  my $addr = do { local $^W; hex $1 };
  my $before = ref($r);
  is( refaddr($r), $addr, $n);
  is( ref($r), $before, $n);

  my $obj = bless $r, 'FooBar';
  is( refaddr($r), $addr, "blessed with overload $n");
  is( ref($r), 'FooBar', $n);
}

{
  my $z = '77';
  my $y = \$z;
  my $a = '78';
  my $b = \$a;
  tie my %x, 'Hash3', {};
  $x{$y} = 22;
  $x{$b} = 23;
  my $xy = $x{$y};
  my $xb = $x{$b}; 
  ok(ref($x{$y}));
  ok(ref($x{$b}));
  ok(refaddr($xy) == refaddr($y));
  ok(refaddr($xb) == refaddr($b));
  ok(refaddr($x{$y}));
  ok(refaddr($x{$b}));
}

package FooBar;

use overload  '0+' => sub { 10 },
		'+' => sub { 10 + $_[1] };

package MyTie;

sub TIEHANDLE { bless {} }
sub DESTROY {}

sub AUTOLOAD {
  warn "$AUTOLOAD called";
  exit 1; # May be in an eval
}

package Hash3;

use Scalar::Util qw(refaddr);

sub TIEHASH
{
	my $pkg = shift;
	return bless [ @_ ], $pkg;
}
sub FETCH
{
	my $self = shift;
	my $key = shift;
	my ($underlying) = @$self;
	return $underlying->{refaddr($key)};
}
sub STORE
{
	my $self = shift;
	my $key = shift;
	my $value = shift;
	my ($underlying) = @$self;
	return ($underlying->{refaddr($key)} = $key);
}
