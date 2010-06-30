# ex:ts=8 sw=4:
# $OpenBSD: PackageLocator.pm,v 1.90 2010/06/30 10:37:25 espie Exp $
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

sub default_path
{
	if (!defined $default_path) {
		$default_path = OpenBSD::PackageRepositoryList->new;

		if (defined $ENV{PKG_PATH}) {
			my $v = $ENV{PKG_PATH};
			$v =~ s/^\:+//o;
			$v =~ s/\:+$//o;
			while (my $o = OpenBSD::PackageRepository->parse(\$v)) {
				$default_path->add($o);
			}
		} else {
			$default_path->add(OpenBSD::PackageRepository->new("./"));
		}

	}
	return $default_path;
}

my $singleton;

sub new
{
	my ($class, $state) = @_;
	return $singleton //= bless { state => $state }, $class;
}

sub path_parse
{
	my ($self, $pkgname, $path) = (@_, './');
	if ($pkgname =~ m/^(.*[\/\:])(.*)/) {
		($pkgname, $path) = ($2, $1);
	}

	return (OpenBSD::PackageRepository->new($path), $pkgname);
}

sub find
{
	my ($self, $_, $arch) = @_;

	my $package;
	if (m/[\/\:]/o) {
		my ($repository, $pkgname) = $self->path_parse($_);
		$package = $repository->find($pkgname, $arch);
		if (defined $package) {
			$self->default_path->add($repository);
		}
	} else {
		$package = $self->default_path->find($_, $arch);
	}
	return $package;
}

sub grabPlist
{
	my ($self, $_, $arch, $code) = @_;

	my $plist;
	if (m/[\/\:]/o) {
		my ($repository, $pkgname) = $self->path_parse($_);
		$plist = $repository->grabPlist($pkgname, $arch, $code);
		if (defined $plist) {
			$self->default_path->add($repository);
		}
	} else {
		$plist = $self->default_path->grabPlist($_, $arch, $code);
	}
	return $plist;
}

sub match_locations
{
	my ($self, @search) = @_;
	return $self->default_path->match_locations(@search);
}

1;
