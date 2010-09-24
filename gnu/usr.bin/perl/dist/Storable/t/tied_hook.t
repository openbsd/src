#!./perl
#
#  Copyright (c) 1995-2000, Raphael Manfredi
#  
#  You may redistribute only under the same terms as Perl 5, as specified
#  in the README file that comes with the distribution.
#

sub BEGIN {
    unshift @INC, 't';
    require Config; import Config;
    if ($ENV{PERL_CORE} and $Config{'extensions'} !~ /\bStorable\b/) {
        print "1..0 # Skip: Storable was not built\n";
        exit 0;
    }
    require 'st-dump.pl';
}

sub ok;

use Storable qw(freeze thaw);

print "1..25\n";

($scalar_fetch, $array_fetch, $hash_fetch) = (0, 0, 0);

package TIED_HASH;

sub TIEHASH {
	my $self = bless {}, shift;
	return $self;
}

sub FETCH {
	my $self = shift;
	my ($key) = @_;
	$main::hash_fetch++;
	return $self->{$key};
}

sub STORE {
	my $self = shift;
	my ($key, $value) = @_;
	$self->{$key} = $value;
}

sub FIRSTKEY {
	my $self = shift;
	scalar keys %{$self};
	return each %{$self};
}

sub NEXTKEY {
	my $self = shift;
	return each %{$self};
}

sub STORABLE_freeze {
	my $self = shift;
	$main::hash_hook1++;
	return join(":", keys %$self) . ";" . join(":", values %$self);
}

sub STORABLE_thaw {
	my ($self, $cloning, $frozen) = @_;
	my ($keys, $values) = split(/;/, $frozen);
	my @keys = split(/:/, $keys);
	my @values = split(/:/, $values);
	for (my $i = 0; $i < @keys; $i++) {
		$self->{$keys[$i]} = $values[$i];
	}
	$main::hash_hook2++;
}

package TIED_ARRAY;

sub TIEARRAY {
	my $self = bless [], shift;
	return $self;
}

sub FETCH {
	my $self = shift;
	my ($idx) = @_;
	$main::array_fetch++;
	return $self->[$idx];
}

sub STORE {
	my $self = shift;
	my ($idx, $value) = @_;
	$self->[$idx] = $value;
}

sub FETCHSIZE {
	my $self = shift;
	return @{$self};
}

sub STORABLE_freeze {
	my $self = shift;
	$main::array_hook1++;
	return join(":", @$self);
}

sub STORABLE_thaw {
	my ($self, $cloning, $frozen) = @_;
	@$self = split(/:/, $frozen);
	$main::array_hook2++;
}

package TIED_SCALAR;

sub TIESCALAR {
	my $scalar;
	my $self = bless \$scalar, shift;
	return $self;
}

sub FETCH {
	my $self = shift;
	$main::scalar_fetch++;
	return $$self;
}

sub STORE {
	my $self = shift;
	my ($value) = @_;
	$$self = $value;
}

sub STORABLE_freeze {
	my $self = shift;
	$main::scalar_hook1++;
	return $$self;
}

sub STORABLE_thaw {
	my ($self, $cloning, $frozen) = @_;
	$$self = $frozen;
	$main::scalar_hook2++;
}

package main;

$a = 'toto';
$b = \$a;

$c = tie %hash, TIED_HASH;
$d = tie @array, TIED_ARRAY;
tie $scalar, TIED_SCALAR;

$scalar = 'foo';
$hash{'attribute'} = 'plain value';
$array[0] = \$scalar;
$array[1] = $c;
$array[2] = \@array;
$array[3] = "plaine scalaire";

@tied = (\$scalar, \@array, \%hash);
%a = ('key', 'value', 1, 0, $a, $b, 'cvar', \$a, 'scalarref', \$scalar);
@a = ('first', 3, -4, -3.14159, 456, 4.5, $d, \$d,
	$b, \$a, $a, $c, \$c, \%a, \@array, \%hash, \@tied);

ok 1, defined($f = freeze(\@a));

$dumped = &dump(\@a);
ok 2, 1;

$root = thaw($f);
ok 3, defined $root;

$got = &dump($root);
ok 4, 1;

ok 5, $got ne $dumped;		# our hooks did not handle refs in array

$g = freeze($root);
ok 6, length($f) == length($g);

# Ensure the tied items in the retrieved image work
@old = ($scalar_fetch, $array_fetch, $hash_fetch);
@tied = ($tscalar, $tarray, $thash) = @{$root->[$#{$root}]};
@type = qw(SCALAR  ARRAY  HASH);

ok 7, tied $$tscalar;
ok 8, tied @{$tarray};
ok 9, tied %{$thash};

@new = ($$tscalar, $tarray->[0], $thash->{'attribute'});
@new = ($scalar_fetch, $array_fetch, $hash_fetch);

# Tests 10..15
for ($i = 0; $i < @new; $i++) {
	ok 10 + 2*$i, $new[$i] == $old[$i] + 1;		# Tests 10,12,14
	ok 11 + 2*$i, ref $tied[$i] eq $type[$i];	# Tests 11,13,15
}

ok 16, $$tscalar eq 'foo';
ok 17, $tarray->[3] eq 'plaine scalaire';
ok 18, $thash->{'attribute'} eq 'plain value';

# Ensure hooks were called
ok 19, ($scalar_hook1 && $scalar_hook2);
ok 20, ($array_hook1 && $array_hook2);
ok 21, ($hash_hook1 && $hash_hook2);

#
# And now for the "blessed ref to tied hash" with "store hook" test...
#

my $bc = bless \%hash, 'FOO';		# FOO does not exist -> no hook
my $bx = thaw freeze $bc;

ok 22, ref $bx eq 'FOO';
my $old_hash_fetch = $hash_fetch;
my $v = $bx->{attribute};
ok 23, $hash_fetch == $old_hash_fetch + 1;	# Still tied

package TIED_HASH_REF;


sub STORABLE_freeze {
        my ($self, $cloning) = @_;
        return if $cloning;
        return('ref lost');
}

sub STORABLE_thaw {
        my ($self, $cloning, $data) = @_;
        return if $cloning;
}

package main;

$bc = bless \%hash, 'TIED_HASH_REF';
$bx = thaw freeze $bc;

ok 24, ref $bx eq 'TIED_HASH_REF';
$old_hash_fetch = $hash_fetch;
$v = $bx->{attribute};
ok 25, $hash_fetch == $old_hash_fetch + 1;	# Still tied
