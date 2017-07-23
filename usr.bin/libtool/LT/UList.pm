# ex:ts=8 sw=4:
# $OpenBSD: UList.pm,v 1.4 2017/07/23 09:47:11 zhuk Exp $
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

use strict;
use warnings;
use feature qw(say);

# Hash that preserves order of adding items and avoids duplicates.
# Also, some additional restrictions are applied to make sure
# the usage of this list is straightforward.

package LT::UList;
require Tie::Array;

our @ISA = qw(Tie::Array);

sub _translate_num_key($$;$) {
	if ($_[1] < 0) {
		$_[1] = @{$_[0]} - (-$_[1]);
		die "invalid index" if $_[1] < 1;
	} else {
		$_[1] += 1;
	}
	die "invalid index $_[1]" if $_[1] - int($_[2] // 0) >= @{$_[0]};
}

# Construct new UList and returnes reference to the array,
# not to the tied object itself.
sub new {
	my $class = shift;
	tie(my @a, $class, @_);
	return \@a;
}

# Given we have successfully added N directories:
#   self->[0] = { directory => 1 }
#   self->[1 .. N] = directories in the order of addition, represented as 0..N-1

sub TIEARRAY {
	my $class = shift;
	my $self = bless [ {} ], $class;
	$self->PUSH(@_);
	return $self;
}

# Unfortunately, exists() checks for the value being integer even in the
# case we have EXISTS() outta there. So if you really need to check the
# presence of particular item, call the method below on the reference
# returned by tie() or tied() instead.
sub exists { return exists $_[0]->[0]->{$_[1]}; }

sub indexof { return exists($_[0]->[0]->{$_[1]}) ? ($_[0]->[0]->{$_[1]} - 1) : undef; }

sub FETCHSIZE { return scalar(@{$_[0]}) - 1; }

# not needed
sub STORE { die "unimplemented and should not be used"; }
sub DELETE { die "unimplemented and should not be used"; }
sub EXTEND { }

sub FETCH
{
	my ($self, $key) = (shift, shift);

	# ignore?
	die "undef given instead of directory or index" unless defined $key;

	$self->_translate_num_key($key);
	return $self->[$key];
}

sub STORESIZE
{
	my ($self, $newsz) = (shift, shift() + 2);
	my $sz = @$self;

	if ($newsz > $sz) {
		# XXX any better way to grow?
		$self->[$newsz - 1] = undef;
	} elsif ($newsz < $sz) {
		$self->POP() for $newsz .. $sz - 1;
	}
}

sub PUSH
{
	my $self = shift;
	for (@_) {
		next if exists $self->[0]->{$_};
		$self->[0]->{$_} = @$self;
		push(@$self, $_);
	}
}

sub POP
{
	my $self = shift;
	return undef if @$self < 2;
	my $key = pop @$self;
	delete $self->[0]->{$key};
	return $key;
}

sub SHIFT
{
	my $self = shift;
	return undef if @$self < 2;
	my $key = splice(@$self, 1, 1);
	delete $self->[0]->{$key};
	return $key;
}

sub UNSHIFT
{
	my $self = shift;
	$self->SPLICE(0, 0, @_);
}

sub SPLICE
{
	my $self = shift;

	my $offset = shift // 0;
	$self->_translate_num_key($offset, 1);
	my $maxrm = @$self - $offset;

	my $length = shift;
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

	# trailing elemenets positions to be renumbered by adding $delta
	my $delta = -$length;

	#
	# First, always remove elements; then add one by one.
	# This way we can be sure to not add duplicates, even if
	# they exist in added elements, e.g., adding ("-lfoo", "-lfoo").
	#

	my @ret = splice(@$self, $offset, $length);
	for (@ret) {
		delete $self->[0]->{$_};
	}

	my $i = 0;
	my %seen;
	for (@_) {
		next if exists $seen{$_};	# skip already added items
		$seen{$_} = 1;
		if (exists $self->[0]->{$_}) {
			if ($self->[0]->{$_} >= $offset + $length) {
				# "move" from tail to new position
				splice(@$self, $self->[0]->{$_} - $length + $i, 1);
			} else {
				next;
			}
		}
		splice(@$self, $offset + $i, 0, $_);
		$self->[0]->{$_} = $offset + $i;
		$i++;
		$delta++;
	}

	for $i ($offset + scalar(@_) .. @$self - 1) {
		$self->[0]->{$self->[$i]} = $i;
	}

	return @ret;
}


=head1 test
package main;

sub compare_ulists {
	my ($list1, $list2) = @_;
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
