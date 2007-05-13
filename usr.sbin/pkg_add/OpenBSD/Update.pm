# ex:ts=8 sw=4:
# $OpenBSD: Update.pm,v 1.71 2007/05/13 11:14:25 espie Exp $
#
# Copyright (c) 2004-2006 Marc Espie <espie@openbsd.org>
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

package OpenBSD::Update;
use OpenBSD::ProgressMeter;
use OpenBSD::Interactive;
use OpenBSD::PackageInfo;
use OpenBSD::PackageLocator;
use OpenBSD::PackageName;
use OpenBSD::Error;

sub new
{
	my $class = shift;
	return bless {cant => [], updates => []}, $class;
}

sub cant
{
	my $self = shift;
	return $self->{cant};
}

sub updates
{
	my $self = shift;
	return $self->{updates};
}

sub add2cant
{
	my ($self, @args) = @_;
	push(@{$self->{cant}}, @args);
}

sub add2updates
{
	my ($self, @args) = @_;
	push(@{$self->{updates}}, @args);
}

sub process_package
{
	my ($self, $pkgname, $state) = @_;
	if ($pkgname =~ m/^(?:\.libs|partial)\-/) {
		OpenBSD::ProgressMeter::clear();
		print "Not updating $pkgname, remember to clean it\n";
		next;
	}
	my $stem = OpenBSD::PackageName::splitstem($pkgname);
	my @l = OpenBSD::PackageLocator->findstem($stem);
	if (@l == 0) {
		$self->add2cant($pkgname);
		return;
	}
	my @l2 = ();
	if (@l > 1 && !$state->{forced}->{allversions}) {
	    @l = OpenBSD::PackageName::keep_most_recent(@l);
	}
	if (@l == 1 && $state->{forced}->{pkgpath}) {
		OpenBSD::ProgressMeter::clear();
		print "Directly updating $pkgname -> ", $l[0], "\n";
		$self->add2updates($l[0]);
		return;
	}
	my $plist = OpenBSD::PackingList->from_installation($pkgname, \&OpenBSD::PackingList::UpdateInfoOnly);
	if (!defined $plist) {
		Fatal("Can't locate $pkgname");
	}
	my $found;
	for my $candidate (@l) {
	    my $handle = OpenBSD::PackageLocator->find($candidate, $state->{arch});
	    if (!$handle) {
		    next;
	    }
	    $handle->close_now;
	    my $p2 = $handle->plist(\&OpenBSD::PackingList::UpdateInfoOnly);
	    if (!$p2) {
		next;
	    }
	    if ($p2->has('arch')) {
		unless ($p2->{arch}->check($state->{arch})) {
		    next;
		}
	    }
	    if ($plist->signature() eq $p2->signature()) {
		$found = $candidate;
	    }
	    if ($p2->{extrainfo}->{subdir} eq $plist->{extrainfo}->{subdir}) {
		push(@l2, $candidate);
	    } elsif ($p2->has('pkgpath')) {
		for my $p (@{$p2->{pkgpath}}) {
			if ($p->{name} eq $plist->{extrainfo}->{subdir}) {
				push(@l2, $candidate);
				last;
			}
		}
	    }
	}

	if (defined $found && @l2 == 1 && $found eq  $l2[0]) {
		if (!$plist->uses_old_libs) {
			my $msg = "No need to update $pkgname";
			OpenBSD::ProgressMeter::message($msg);
			print "$msg\n" if $state->{beverbose};
			return;
		}
	}
	OpenBSD::ProgressMeter::clear();
	print "Candidates for updating $pkgname -> ", join(' ', @l2), "\n";
	# if all packages have the same version, but distinct p,
	# grab the most recent.
	if (@l2 > 1) {
	    @l2 = OpenBSD::PackageName::keep_most_recent(@l2);
	}
		
	if (@l2 == 1) {
		if (defined $found && $found eq  $l2[0] && !$plist->uses_old_libs) {
			my $msg = "No need to update $pkgname";
			OpenBSD::ProgressMeter::message($msg);
			print "$msg\n" if $state->{beverbose};
		} else {
			$self->add2updates($l2[0]);
		}
	} elsif (@l2 == 0) {
		$self->add2cant($pkgname);
	} else {
		my $result = OpenBSD::Interactive::choose1($pkgname, $state->{interactive}, sort @l2);
		if (defined $result) {
			if (defined $found && $found eq  $result && !$plist->uses_old_libs) {
				print "No need to update $pkgname\n";
			} else {
				$self->add2updates($result);
			}
		} else {
			$state->{issues} = 1;
		}
	}
}

sub process
{
	my ($self, $old, $state) = @_;
	my @list = ();

	OpenBSD::PackageInfo::solve_installed_names($old, \@list, "(updating them all)", $state);
	unless (defined $state->{full_update} or defined $state->{forced}->{noclosure}) {
		require OpenBSD::RequiredBy;

		@list = OpenBSD::Requiring->compute_closure(@list);
	}

	OpenBSD::ProgressMeter::set_header("Looking for updates");
	for my $pkgname (@list) {
		$self->process_package($pkgname, $state);
	}
	OpenBSD::ProgressMeter::next();
}

1;
