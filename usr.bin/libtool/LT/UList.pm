# ex:ts=8 sw=4:
# $OpenBSD: UList.pm,v 1.3 2016/12/25 13:46:18 zhuk Exp $
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
	die "invalid index" if $_[1] - int($_[2] // 0) >= @{$_[0]};
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
		$length = $maxrm - (-$length) if $length < 0;
		$length = $maxrm if $length > $maxrm;
	} else {
		$length = $maxrm;
	}

	my $i = @$self;

	# make sure no duplicates get added
	@_ = grep { !exists $self->[0] or
		    $self->[0]->{$_} >= $offset &&
	            $self->[0]->{$_} < $offset + $length } @_;

	for (@_) {
		# set up index
		$self->[0]->{$_} = $i++;
	}

	#
	# Renumber (in advance) trailing items, in case something gets added
	# and number of added and removed items differs.
	#
	my $delta = scalar(@_) - $length;
	if (scalar(@_) and $delta) {
		for $i ($offset + $length .. scalar(@$self)) {
			$self->[0]->{$self->[$i]} += $delta;
		}
	}

	my @ret = splice(@$self, $offset, $length, @_);

	for (@ret) {
		delete $self->[0]->{$_};
	}
	for ($offset .. scalar(@$self) - 1) {
		$self->[0]->{$self->[$_]} -= $length;
	}

	return @ret unless scalar(@_);

	if ($length == $maxrm) {
		# simply add items to the end
		$self->PUSH(@_);
		return @ret;
	}

	my $newpos = $offset;
	for (@_) {
		my $index = $self->[0]->{$_};
		if (defined $index) {
			if ($index < $offset) {
				# skip current item totally
				continue;
			} elsif ($index == $offset) {
				# skip adding but act as if added
				$self->[0]->{$_} += $newpos - $offset;
				$newpos++;
				next;
			}
			splice(@$self, $index, 1);
		}
		splice(@$self, $newpos, 0, $_);
		$self->[0]->{$_} = $newpos++;
	}
	for ($newpos .. scalar(@$self) - 1) {
		$self->[0]->{$self->[$_]} += $newpos - $offset;
	}
	return @ret;
}


=head1 test
package main;

my $r = ['/path0', '/path1'];
tie(@$r, 'LT::UList');
#push(@$r, '/path0');
#push(@$r, '/path1');
push(@$r, '/path2');
push(@$r, '/path3');
push(@$r, '/path4');
push(@$r, '/path3');
push(@$r, '/path1');
push(@$r, '/path5');
say "spliced: ".join(", ", splice(@$r, 2, 2, '/pathAdd1', '/pathAdd2', '/pathAdd1'));
#say "a: ".join(", ", @a);
say "r: ".join(", ", @$r);
#say "r2: ".join(", ", @$r2);
=cut
