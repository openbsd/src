# ex:ts=8 sw=4:
# $OpenBSD: PackageRepositoryList.pm,v 1.22 2010/05/10 09:17:55 espie Exp $
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
	return bless {l => [], c => {}, k => {}}, $class;
}

sub add
{
	my $self = shift;
	for my $r (@_) {
		next if $self->{k}{$r};
		push @{$self->{l}}, $r;
	}
}

sub do_something
{
	my ($self, $do, $pkgname, @args) = @_;
	if ($pkgname eq '-') {
		return OpenBSD::PackageRepository::Local::Pipe->new->$do($pkgname, @args);
	}
	for my $repo (@{$self->{l}}) {
		my $r = $repo->$do($pkgname, @args);
		return $r if defined $r;
	}
	return undef;
}

sub find
{
	my ($self, @args) = @_;

	return $self->do_something('find', @args);
}

sub grabPlist
{
	my ($self, @args) = @_;
	return $self->do_something('grabPlist', @args);
}

sub match_locations
{
	my ($self, @search) = @_;
	for my $repo (@{$self->{l}}) {
		my $l = $repo->match_locations(@search);
		if (@$l > 0) {
			return $l;
		}
	}
	return [];
}

sub print_without_src
{
	my $self = shift;
	my @l = ();
	for my $repo (@$self) {
		next if $repo->isa("OpenBSD::PackageRepository::Source");
		push(@l, $repo->url);
	}
	return join(':', @l);
}

1;
