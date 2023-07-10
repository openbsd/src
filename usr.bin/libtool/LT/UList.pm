# ex:ts=8 sw=4:
# $OpenBSD: UList.pm,v 1.7 2023/07/10 09:29:48 espie Exp $
#
# Copyright (c) 2013 Vadim Zhukov <zhuk@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

use v5.36;

# Hash that preserves order of adding items and avoids duplicates.
# Also, some additional restrictions are applied to make sure
# the usage of this list is straightforward.

package LT::UList;
require Tie::Array;

our @ISA = qw(Tie::Array);

sub _translate_num_key($self, $idx, $offset = 0)
{
	if ($idx < 0) {
		$idx += @$self;
		die "invalid index" if $idx < 1;
	} else {
		$idx++;
	}
	die "invalid index $idx" if $idx - int($offset) >= @$self;
	return $idx;
}


# Construct new UList and returns reference to the array,
# not to the tied object itself.
sub new ($class, @p)
{
	tie(my @a, $class, @p);
	return \@a;
}

# Given we have successfully added N directories:
#   self->[0] = { directory => 1 }
#   self->[1 .. N] = directories in the order of addition, represented as 0..N-1

sub TIEARRAY($class, @p)
{
	my $self = bless [ {} ], $class;
	$self->PUSH(@p);
	return $self;
}

# Unfortunately, exists() checks for the value being integer even in the
# case we have EXISTS() outta there. So if you really need to check the
# presence of particular item, call the method below on the reference
# returned by tie() or tied() instead.
sub exists($self, $key)
{ 
	return exists $self->[0]{$key}; 
}

sub indexof($self, $key)
{ 
	return exists($self->[0]{$key}) ? ($self->[0]{$key} - 1) : undef; 
}

sub FETCHSIZE($self)
{ 
	return scalar(@$self) - 1; 
}

sub STORE($, $, $)
{ 
	die "overwriting elements is unimplemented";
}

sub DELETE($, $)
{ 	
	die "delete is unimplemented";
}


sub FETCH($self, $key)
{
	return $self->[$self->_translate_num_key($key)];
}

sub STORESIZE($self, $newsz)
{
	$newsz += 2;
	my $sz = @$self;

	if ($newsz > $sz) {
		# XXX any better way to grow?
		$self->[$newsz - 1] = undef;
	} elsif ($newsz < $sz) {
		$self->POP for $newsz .. $sz - 1;
	}
}

sub PUSH($self, @p)
{
	for (@p) {
		next if exists $self->[0]{$_};
		$self->[0]{$_} = @$self;
		push(@$self, $_);
	}
}

sub POP($self)
{
	return undef if @$self < 2;
	my $key = pop @$self;
	delete $self->[0]{$key};
	return $key;
}

sub SHIFT($self)
{
	return undef if @$self < 2;
	my $key = splice(@$self, 1, 1);
	delete $self->[0]{$key};
	return $key;
}

sub UNSHIFT($self, @p)
{
	$self->SPLICE(0, 0, @p);
}

sub SPLICE($self, $offset = 0, $length = undef, @p)
{
	$offset = $self->_translate_num_key($offset, 1);
	my $maxrm = @$self - $offset;

	if (defined $length) {
		if ($length < 0) {
			$length = $maxrm - (-$length);
			$length = 0 if $length < 0;
		} elsif ($length > $maxrm) {
			$length = $maxrm;
		}
	} else {
		$length = $maxrm;
	}

	# trailing elements positions to be renumbered by adding $delta
	my $delta = -$length;

	#
	# First, always remove elements; then add one by one.
	# This way we can be sure to not add duplicates, even if
	# they exist in added elements, e.g., adding ("-lfoo", "-lfoo").
	#

	my @ret = splice(@$self, $offset, $length);
	for (@ret) {
		delete $self->[0]{$_};
	}

	my $i = 0;
	my %seen;
	for (@p) {
		next if exists $seen{$_};	# skip already added items
		$seen{$_} = 1;
		if (exists $self->[0]{$_}) {
			if ($self->[0]{$_} >= $offset + $length) {
				# "move" from tail to new position
				splice(@$self, $self->[0]{$_} - $length + $i, 1);
			} else {
				next;
			}
		}
		splice(@$self, $offset + $i, 0, $_);
		$self->[0]{$_} = $offset + $i;
		$i++;
		$delta++;
	}

	for $i ($offset + scalar(@p) .. @$self - 1) {
		$self->[0]{$self->[$i]} = $i;
	}

	return @ret;
}


=head1 test
package main;

sub compare_ulists($list1, $list2) {
	return 0 if scalar(@$list1) != scalar(@$list2);
	for my $i (0 .. scalar(@$list1) - 1) {
		return 0 if $list1->[$i] ne $list2->[$i];
	}
	return 1;
}

my $r = ['/path0', '/path1'];
tie(@$r, 'LT::UList');
push(@$r, '/path0');
push(@$r, '/path1');
push(@$r, '/path2');
push(@$r, '/path3');
push(@$r, '/path4');
push(@$r, '/path3');
push(@$r, '/path1');
push(@$r, '/path5');

my @tests = (
	# offset, length, args,
	# expected resulting array

	[
		3, 0, [],
		['/path0', '/path1', '/path2', '/path3', '/path4', '/path5']
	],

	[
		3, 2, [],
		['/path0', '/path1', '/path2', '/path5']
	],

	[
		0, 3, ['/path0', '/path1', '/path2'],
		['/path0', '/path1', '/path2', '/path5']
	],

	[
		0, 3, ['/path0', '/path5', '/path5', '/path2'],
		['/path0', '/path5', '/path2']
	],

	[
		0, 3, [],
		[]
	],

);

for my $t (@tests) {
	splice(@$r, $t->[0], $t->[1], @{$t->[2]});
	if (!compare_ulists($r, $t->[3])) {
		say "expected: ".join(", ", @{$t->[2]});
		say "     got: ".join(", ", @$r);
		exit 1;
	}
}
exit 0;
=cut
