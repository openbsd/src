# ex:ts=8 sw=4:
# $OpenBSD: Update.pm,v 1.84 2009/06/06 11:48:04 espie Exp $
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
	if ($pkgname =~ m/^(?:\.libs\d*|partial)\-/o) {
		$state->progress->clear;
		print "Not updating $pkgname, remember to clean it\n";
		return;
	}
	my @search = ();
	push(@search, OpenBSD::Search::Stem->split($pkgname));
	my $found;
	my $plist;

	push(@search, OpenBSD::Search::FilterLocation->new(
	    sub {
		my $l = shift;
		if (@$l == 0) {
			return $l;
		}
		if (@$l == 1 && $state->{defines}->{pkgpath}) {
			return $l;
		}
		my @l2 = ();
		$plist = OpenBSD::PackingList->from_installation($pkgname, \&OpenBSD::PackingList::UpdateInfoOnly);
		if (!defined $plist) {
			Fatal("Can't locate $pkgname");
		}
		for my $handle (@$l) {
		    $handle->set_arch($state->{arch});
		    if (!$handle) {
			    next;
		    }
		    my $p2 = $handle->update_info;
		    if (!$p2) {
			next;
		    }
		    if ($p2->has('arch')) {
			unless ($p2->{arch}->check($state->{arch})) {
			    next;
			}
		    }
		    if ($plist->signature eq $p2->signature) {
			$found = $handle;
			push(@l2, $handle);
			next;
		    }
		    if ($plist->match_pkgpath($p2)) {
			push(@l2, $handle);
		    }
		}
		return \@l2;
	    }));

	if (!$state->{defines}->{allversions}) {
		push(@search, OpenBSD::Search::FilterLocation->keep_most_recent);
	}


	my $l = OpenBSD::PackageLocator->match_locations(@search);
	if (@$l == 0) {
		$self->add2cant($pkgname);
		return;
	}
	if (@$l == 1) {
		if ($state->{defines}->{pkgpath}) {
			$state->progress->clear;
			print "Directly updating $pkgname -> ", $l->[0]->name, "\n";
			$self->add2updates($l->[0]);
			return;
		}
		if (defined $found && $found eq  $l->[0] && 
		    !$plist->uses_old_libs && !$state->{defines}->{installed}) {
				my $msg = "No need to update $pkgname";
				$state->progress->message($msg);
				print "$msg\n" if $state->{beverbose};
				return;
		}
	}

	$state->progress->clear;
	my %cnd = map {($_->name, $_)} @$l;
	print "Candidates for updating $pkgname -> ", join(' ', keys %cnd), "\n";
		
	if (@$l == 1) {
		$self->add2updates($l->[0]);
		return;
	}
	my $result = OpenBSD::Interactive::choose1($pkgname, 
	    $state->{interactive}, sort keys %cnd);
	if (defined $result) {
		if (defined $found && $found eq $result && 
		    !$plist->uses_old_libs) {
			print "No need to update $pkgname\n";
		} else {
			$self->add2updates($cnd{$result});
		}
	} else {
		$state->{issues} = 1;
	}
}

sub process
{
	my ($self, $old, $state) = @_;
	my @list = ();

	OpenBSD::PackageInfo::solve_installed_names($old, \@list, "(updating them all)", $state);
	unless (defined $state->{full_update} or defined $state->{defines}->{noclosure}) {
		require OpenBSD::RequiredBy;

		@list = OpenBSD::Requiring->compute_closure(@list);
	}

	$state->progress->set_header("Looking for updates");
	for my $pkgname (@list) {
		$self->process_package($pkgname, $state);
	}
	$state->progress->next;
}

1;
