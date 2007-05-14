# ex:ts=8 sw=4:
# $OpenBSD: PkgCfl.pm,v 1.20 2007/05/14 10:53:31 espie Exp $
#
# Copyright (c) 2003-2005 Marc Espie <espie@openbsd.org>
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

package OpenBSD::PkgCfl;
use OpenBSD::PackageName;
use OpenBSD::PkgSpec;
use OpenBSD::PackageInfo;

sub make_conflict_list
{
	my ($class, $plist, $pkg) = @_;
	my $l = [];
	my $pkgname = $plist->pkgname;
	if (!defined $pkgname) {
		print STDERR "No pkgname in packing-list for $pkg\n";
		return;
	}
	my $stem = OpenBSD::PackageName::splitstem($pkgname);

	unless (defined $plist->{'no-default-conflict'}) {
		push(@$l, OpenBSD::Search::PkgSpec->new("$stem-*|partial-$stem-*"));
	} else {
		$pkgname =~ s/p\d+$//;
		push(@$l, OpenBSD::Search::PkgSpec->new("$pkgname|partial-$pkgname"));
	}
	push(@$l, OpenBSD::Search::PkgSpec->new(".libs-$stem-*"));
	if (defined $plist->{conflict}) {
		for my $cfl (@{$plist->{conflict}}) {
		    push(@$l, OpenBSD::Search::PkgSpec->new($cfl->{name}));
		}
	}
	bless $l, $class;
}

sub conflicts_with
{
	my ($self, @pkgnames) = @_;
	my @l = ();
	for my $cfl (@$self) {
		push(@l, $cfl->match_list(@pkgnames));
	}
	return @l;
}

sub register($$)
{
	my ($plist, $state) = @_;
	if (!defined $plist->{conflicts}) {
		$plist->{conflicts} = OpenBSD::PkgCfl->make_conflict_list($plist);
	}
	$state->{conflict_list}->{$plist->pkgname} = $plist->{conflicts};
}

sub unregister($$)
{
	my ($plist, $state) = @_;
	delete $state->{conflict_list}->{$plist->pkgname};
}

sub fill_conflict_lists($)
{
	my $state = shift;
	for my $pkg (installed_packages()) {
		my $plist = OpenBSD::PackingList->from_installation($pkg, 
		    \&OpenBSD::PackingList::ConflictOnly);
		next unless defined $plist;
		$plist->{conflicts} = OpenBSD::PkgCfl->make_conflict_list($plist, $pkg);
		register($plist, $state);
	}
}

sub find($$)
{
	my ($pkgname, $state) = @_;
	my @bad = ();
	if (is_installed $pkgname) {
		push(@bad, $pkgname);
	}
	if (!defined $state->{conflict_list}) {
		$state->{conflict_list} = {};
		fill_conflict_lists($state);
	}
	while (my ($name, $l) = each %{$state->{conflict_list}}) {
		next if $name eq $pkgname;
		if (!defined $l) {
			die "Error: $name has no definition\n";
		}
		if ($l->conflicts_with($pkgname)) {
			push(@bad, $name);
		}
	}
	return @bad;
}

sub find_all
{
	my ($plist, $state) = @_;
	my $pkgname = $plist->pkgname();

	my $l = OpenBSD::PkgCfl->make_conflict_list($plist);
	$plist->{conflicts} = $l;

	my @conflicts = find($pkgname, $state);
	push(@conflicts, $l->conflicts_with(installed_packages()));
	return @conflicts;
}

1;
