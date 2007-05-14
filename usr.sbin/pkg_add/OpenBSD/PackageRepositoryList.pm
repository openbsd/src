# ex:ts=8 sw=4:
# $OpenBSD: PackageRepositoryList.pm,v 1.13 2007/05/14 12:49:27 espie Exp $
#
# Copyright (c) 2003-2006 Marc Espie <espie@openbsd.org>
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

package OpenBSD::PackageRepositoryList;

sub new
{
	my $class = shift;
	return bless {list => [], avail => undef }, $class;
}

sub add
{
	my $self = shift;
	push @{$self->{list}}, @_;
	if (@_ > 0) {
		$self->{avail} = undef;
	}
}

sub find
{
	my ($self, $pkgname, $arch, $srcpath) = @_;

	for my $repo (@{$self->{list}}) {
		my $pkg = $repo->find($pkgname, $arch, $srcpath);
		return $pkg if defined $pkg;
	}
	return;
}

sub grabPlist
{
	my ($self, $pkgname, $arch, $code) = @_;

	for my $repo (@{$self->{list}}) {
		my $plist = $repo->grabPlist($pkgname, $arch, $code);
		return $plist if defined $plist;
	}
	return;
}

sub available
{
	my $self = shift;

	if (!defined $self->{avail}) {
		my $available_packages = {};
		foreach my $loc (reverse @{$self->{list}}) {
		    foreach my $pkg (@{$loc->list}) {
		    	$available_packages->{$pkg} = $loc;
		    }
		}
		$self->{avail} = $available_packages;
	}
	return keys %{$self->{avail}};
}

sub match
{
	my ($self, @search) = @_;
	for my $repo (@{$self->{list}}) {
		my @l = $repo->match(@search);
		if (@l > 0) {
			return @l;
		}
	}
	return ();
}

sub cleanup
{
	my $self = shift;
	for my $repo (@{$self->{list}}) {
		$repo->cleanup;
	}
}

1;
