#!./perl
#
#  Copyright (c) 1995-2000, Raphael Manfredi
#  
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#  

sub BEGIN {
    unshift @INC, 't';
    unshift @INC, 't/compat' if $] < 5.006002;
    require Config; import Config;
    if ($ENV{PERL_CORE} and $Config{'extensions'} !~ /\bStorable\b/) {
        print "1..0 # Skip: Storable was not built\n";
        exit 0;
    }
}

use Storable qw(freeze thaw dclone);
use Test::More tests => 33;

package OBJ_REAL;

use Storable qw(freeze thaw);

@x = ('a', 1);

sub make { bless [], shift }

sub STORABLE_freeze {
	my $self = shift;
	my $cloning = shift;
	die "STORABLE_freeze" unless Storable::is_storing;
	return (freeze(\@x), $self);
}

sub STORABLE_thaw {
	my $self = shift;
	my $cloning = shift;
	my ($x, $obj) = @_;
	die "STORABLE_thaw #1" unless $obj eq $self;
	my $len = length $x;
	my $a = thaw $x;
	die "STORABLE_thaw #2" unless ref $a eq 'ARRAY';
	die "STORABLE_thaw #3" unless @$a == 2 && $a->[0] eq 'a' && $a->[1] == 1;
	@$self = @$a;
	die "STORABLE_thaw #4" unless Storable::is_retrieving;
}

package OBJ_SYNC;

@x = ('a', 1);

sub make { bless {}, shift }

sub STORABLE_freeze {
	my $self = shift;
	my ($cloning) = @_;
	return if $cloning;
	return ("", \@x, $self);
}

sub STORABLE_thaw {
	my $self = shift;
	my ($cloning, $undef, $a, $obj) = @_;
	die "STORABLE_thaw #1" unless $obj eq $self;
	die "STORABLE_thaw #2" unless ref $a eq 'ARRAY' || @$a != 2;
	$self->{ok} = $self;
}

package OBJ_SYNC2;

use Storable qw(dclone);

sub make {
	my $self = bless {}, shift;
	my ($ext) = @_;
	$self->{sync} = OBJ_SYNC->make;
	$self->{ext} = $ext;
	return $self;
}

sub STORABLE_freeze {
	my $self = shift;
	my %copy = %$self;
	my $r = \%copy;
	my $t = dclone($r->{sync});
	return ("", [$t, $self->{ext}], $r, $self, $r->{ext});
}

sub STORABLE_thaw {
	my $self = shift;
	my ($cloning, $undef, $a, $r, $obj, $ext) = @_;
	die "STORABLE_thaw #1" unless $obj eq $self;
	die "STORABLE_thaw #2" unless ref $a eq 'ARRAY';
	die "STORABLE_thaw #3" unless ref $r eq 'HASH';
	die "STORABLE_thaw #4" unless $a->[1] == $r->{ext};
	$self->{ok} = $self;
	($self->{sync}, $self->{ext}) = @$a;
}

package OBJ_REAL2;

use Storable qw(freeze thaw);

$MAX = 20;
$recursed = 0;
$hook_called = 0;

sub make { bless [], shift }

sub STORABLE_freeze {
	my $self = shift;
	$hook_called++;
	return (freeze($self), $self) if ++$recursed < $MAX;
	return ("no", $self);
}

sub STORABLE_thaw {
	my $self = shift;
	my $cloning = shift;
	my ($x, $obj) = @_;
	die "STORABLE_thaw #1" unless $obj eq $self;
	$self->[0] = thaw($x) if $x ne "no";
	$recursed--;
}

package main;

my $real = OBJ_REAL->make;
my $x = freeze $real;
isnt($x, undef);

my $y = thaw $x;
is(ref $y, 'OBJ_REAL');
is($y->[0], 'a');
is($y->[1], 1);

my $sync = OBJ_SYNC->make;
$x = freeze $sync;
isnt($x, undef);

$y = thaw $x;
is(ref $y, 'OBJ_SYNC');
is($y->{ok}, $y);

my $ext = [1, 2];
$sync = OBJ_SYNC2->make($ext);
$x = freeze [$sync, $ext];
isnt($x, undef);

my $z = thaw $x;
$y = $z->[0];
is(ref $y, 'OBJ_SYNC2');
is($y->{ok}, $y);
is(ref $y->{sync}, 'OBJ_SYNC');
is($y->{ext}, $z->[1]);

$real = OBJ_REAL2->make;
$x = freeze $real;
isnt($x, undef);
is($OBJ_REAL2::recursed, $OBJ_REAL2::MAX);
is($OBJ_REAL2::hook_called, $OBJ_REAL2::MAX);

$y = thaw $x;
is(ref $y, 'OBJ_REAL2');
is($OBJ_REAL2::recursed, 0);

$x = dclone $real;
isnt($x, undef);
is(ref $x, 'OBJ_REAL2');
is($OBJ_REAL2::recursed, 0);
is($OBJ_REAL2::hook_called, 2 * $OBJ_REAL2::MAX);

is(Storable::is_storing, '');
is(Storable::is_retrieving, '');

#
# The following was a test-case that Salvador Ortiz Garcia <sog@msg.com.mx>
# sent me, along with a proposed fix.
#

package Foo;

sub new {
	my $class = shift;
	my $dat = shift;
	return bless {dat => $dat}, $class;
}

package Bar;
sub new {
	my $class = shift;
	return bless {
		a => 'dummy',
		b => [ 
			Foo->new(1),
			Foo->new(2), # Second instance of a Foo 
		]
	}, $class;
}

sub STORABLE_freeze {
	my($self,$clonning) = @_;
	return "$self->{a}", $self->{b};
}

sub STORABLE_thaw {
	my($self,$clonning,$dummy,$o) = @_;
	$self->{a} = $dummy;
	$self->{b} = $o;
}

package main;

my $bar = new Bar;
my $bar2 = thaw freeze $bar;

is(ref($bar2), 'Bar');
is(ref($bar->{b}[0]), 'Foo');
is(ref($bar->{b}[1]), 'Foo');
is(ref($bar2->{b}[0]), 'Foo');
is(ref($bar2->{b}[1]), 'Foo');

#
# The following attempts to make sure blessed objects are blessed ASAP
# at retrieve time.
#

package CLASS_1;

sub make {
	my $self = bless {}, shift;
	return $self;
}

package CLASS_2;

sub make {
	my $self = bless {}, shift;
	my ($o) = @_;
	$self->{c1} = CLASS_1->make();
	$self->{o} = $o;
	$self->{c3} = bless CLASS_1->make(), "CLASS_3";
	$o->set_c2($self);
	return $self;
}

sub STORABLE_freeze {
	my($self, $clonning) = @_;
	return "", $self->{c1}, $self->{c3}, $self->{o};
}

sub STORABLE_thaw {
	my($self, $clonning, $frozen, $c1, $c3, $o) = @_;
	main::is(ref $self, "CLASS_2");
	main::is(ref $c1, "CLASS_1");
	main::is(ref $c3, "CLASS_3");
	main::is(ref $o, "CLASS_OTHER");
	$self->{c1} = $c1;
	$self->{c3} = $c3;
}

package CLASS_OTHER;

sub make {
	my $self = bless {}, shift;
	return $self;
}

sub set_c2 { $_[0]->{c2} = $_[1] }

#
# Is the reference count of the extra references returned from a
# STORABLE_freeze hook correct? [ID 20020601.005]
#
package Foo2;

sub new {
	my $self = bless {}, $_[0];
	$self->{freezed} = "$self";
	return $self;
}

sub DESTROY {
	my $self = shift;
	$::refcount_ok = 1 unless "$self" eq $self->{freezed};
}

package Foo3;

sub new {
	bless {}, $_[0];
}

sub STORABLE_freeze {
	my $obj = shift;
	return ("", $obj, Foo2->new);
}

sub STORABLE_thaw { } # Not really used

package main;
use vars qw($refcount_ok);

my $o = CLASS_OTHER->make();
my $c2 = CLASS_2->make($o);
my $so = thaw freeze $o;

$refcount_ok = 0;
thaw freeze(Foo3->new);
is($refcount_ok, 1);
