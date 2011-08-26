# ex:ts=8 sw=4:
# $OpenBSD: PackageLocator.pm,v 1.99 2011/08/26 08:46:10 espie Exp $
#
# Copyright (c) 2003-2010 Marc Espie <espie@openbsd.org>
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

package OpenBSD::PackageLocator;

use OpenBSD::PackageRepositoryList;
use OpenBSD::PackageRepository;

my $default_path;

sub build_default_path
{
	my ($self, $state) = @_;
	$default_path = OpenBSD::PackageRepositoryList->new($state);

	if (defined $ENV{PKG_PATH}) {
		my $v = $ENV{PKG_PATH};
		$v =~ s/^\:+//o;
		$v =~ s/\:+$//o;
		while (my $o = OpenBSD::PackageRepository->parse(\$v, $state)) {
			$default_path->add($o);
		}
		return;
	}
	$default_path->add(OpenBSD::PackageRepository->new("./", $state)->can_be_empty);
	if (my $i = $state->config->value("installpath")) {
		$default_path->add(OpenBSD::PackageRepository->new($i, $state));
	}
}

sub default_path
{
	if (!defined $default_path) {
		&build_default_path;
	}
	return $default_path;
}

sub path_parse
{
	my ($self, $pkgname, $state, $path) = (@_, './');
	if ($pkgname =~ m/^(.*[\/\:])(.*)/) {
		($pkgname, $path) = ($2, $1);
	}

	return (OpenBSD::PackageRepository->new($path, $state), $pkgname);
}

sub find
{
	my ($self, $_, $state) = @_;

	my $package;
	if (m/[\/\:]/o) {
		my ($repository, $pkgname) = $self->path_parse($_, $state);
		$package = $repository->find($pkgname);
		if (defined $package) {
			$self->default_path($state)->add($repository);
		}
	} else {
		$package = $self->default_path($state)->find($_);
	}
	return $package;
}

sub grabPlist
{
	my ($self, $_, $code, $state) = @_;

	my $plist;
	if (m/[\/\:]/o) {
		my ($repository, $pkgname) = $self->path_parse($_, $state);
		$plist = $repository->grabPlist($pkgname, $code);
		if (defined $plist) {
			$self->default_path($state)->add($repository);
		}
	} else {
		$plist = $self->default_path($state)->grabPlist($_, $code);
	}
	return $plist;
}

sub match_locations
{
	my ($self, @search) = @_;
	my $state = pop @search;
	return $self->default_path($state)->match_locations(@search);
}

1;
