# ex:ts=8 sw=4:
# $OpenBSD: PackageLocator.pm,v 1.69 2007/05/14 12:52:53 espie Exp $
#
# Copyright (c) 2003-2007 Marc Espie <espie@openbsd.org>
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

# this returns an archive handle from an uninstalled package name, currently
# There is a cache available.

my %packages;
my $pkgpath = OpenBSD::PackageRepositoryList->new;

if (defined $ENV{PKG_PATH}) {
	my $v = $ENV{PKG_PATH};
	$v =~ s/^\:+//;
	$v =~ s/\:+$//;
	my @tentative = split /\/\:/, $v;
	while (my $i = shift @tentative) {
		$i =~ m|/$| or $i.='/';
		$pkgpath->add(OpenBSD::PackageRepository->new($i));
	}
} else {
	$pkgpath->add(OpenBSD::PackageRepository->new("./"));
}

sub path_parse
{
	use File::Basename;

	my ($pkgname, $path) = fileparse($_);
	my $repository = OpenBSD::PackageRepository->new($path);
	return ($repository, $path, $pkgname);
}

sub find
{
	my $class = shift;
	local $_ = shift;
	my $arch = shift;
	my $srcpath = shift;

	if ($_ eq '-') {
		my $repository = OpenBSD::PackageRepository::Local::Pipe->_new('./');
		my $package = $repository->find(undef, $arch, $srcpath);
		return $package;
	}
	if (exists $packages{$_}) {
		return $packages{$_};
	}
	my $package;
	if (m/\//) {
		my ($repository, undef, $pkgname) = path_parse($_);
		$package = $repository->find($pkgname, $arch, $srcpath);
		if (defined $package) {
			$pkgpath->add($repository);
		}
	} else {
		$package = $pkgpath->find($_, $arch, $srcpath);
	}
	$packages{$_} = $package if defined($package);
	return $package;
}

sub grabPlist
{
	my $class = shift;
	local $_ = shift;
	my $arch = shift;
	my $code = shift;

	if ($_ eq '-') {
		my $repository = OpenBSD::PackageRepository::Local::Pipe->_new('./');
		my $plist = $repository->grabPlist(undef, $arch, $code);
		return $plist;
	}
	my $plist;
	if (m/\//) {
		my ($repository, undef, $pkgname) = path_parse($_);
		$plist = $repository->grabPlist($pkgname, $arch, $code);
		if (defined $plist) {
			$pkgpath->add($repository);
		}
	} else {
		$plist = $pkgpath->grabPlist($_, $arch, $code);
	}
	return $plist;
}

sub cleanup
{
	$pkgpath->cleanup;
}

sub match
{
	my ($class, @search) = @_;
	return $pkgpath->match(@search);
}

1;
