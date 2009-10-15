# ex:ts=8 sw=4:
# $OpenBSD: Tracker.pm,v 1.1 2009/10/15 18:17:18 espie Exp $
#
# Copyright (c) 2009 Marc Espie <espie@openbsd.org>
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
#

use strict;
use warnings;

# the tracker class is used to track what's going on during a complicated
# install. Specifically: what packages are installed, what's left to do,
# etc

package OpenBSD::Tracker;
sub new
{
	my $class = shift;
	return bless {}, $class;
}

sub add_set
{
	my ($self, $set) = @_;
	for my $n ($set->newer) {
		$self->{to_install}->{$n->pkgname} = $set;
	}
	for my $n ($set->older) {
		$self->{to_update}->{$n->pkgname} = $set;
	}
	return $self;
}

sub add_sets
{
	my ($self, @sets) = @_;
	for my $set (@sets) {
		$self->add_set($set);
	}
	return $self;
}

sub mark_installed
{
	my ($self, $set) = @_;
	for my $n ($set->newer) {
		undef $self->{to_install}->{$n->pkgname};
		$self->{installed}->{$n->pkgname} = 1;
	}
	for my $n ($set->older) {
		undef $self->{to_update}->{$n->pkgname};
	}
}

1;
