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


use Scalar::Util qw(refaddr);
use vars qw($t $y $x *F $v $r);
use Symbol qw(gensym);

# Ensure we do not trigger and tied methods
tie *F, 'MyTie';

print "1..19\n";

my $i = 1;
foreach $v (undef, 10, 'string') {
  print "not " if defined refaddr($v);
  print "ok ",$i++,"\n";
}

foreach $r ({}, \$t, [], \*F, sub {}) {
  my $addr = $r + 0;
  print "not " unless refaddr($r) == $addr;
  print "ok ",$i++,"\n";
  my $obj = bless $r, 'FooBar';
  print "not " unless refaddr($r) == $addr;
  print "ok ",$i++,"\n";
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
  print "not " unless ref($x{$y});
  print "ok ",$i++,"\n";
  print "not " unless ref($x{$b});
  print "ok ",$i++,"\n";
  print "not " unless refaddr($xy) == refaddr($y);
  print "ok ",$i++,"\n";
  print "not " unless refaddr($xb) == refaddr($b);
  print "ok ",$i++,"\n";
  print "not " unless refaddr($x{$y});
  print "ok ",$i++,"\n";
  print "not " unless refaddr($x{$b});
  print "ok ",$i++,"\n";
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
