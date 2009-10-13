# ex:ts=8 sw=4:
# $OpenBSD: Update.pm,v 1.85 2009/10/13 21:21:07 espie Exp $
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
use OpenBSD::UpdateSet;

sub new
{
	my $class = shift;
	return bless {}, $class;
}

sub add_updateset
{
	my ($self, $set, $handle, $location) = @_;
 
	my $n = OpenBSD::Handle->from_location($location);
	$set->add_newer($n);
}

sub process_handle
{
	my ($self, $set, $h, $state) = @_;
	my $pkgname = $h->{pkgname};
	if (defined $h->{update}) {
		$state->progress->clear;
		print "Update to $pkgname already found\n";
		return 0;
	}
 
	if ($pkgname =~ m/^(?:\.libs\d*|partial)\-/o) {
		$state->progress->clear;
		print "Not updating $pkgname, remember to clean it\n";
		return 0;
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
		return undef;
	}
	if (@$l == 1) {
		if ($state->{defines}->{pkgpath}) {
			$state->progress->clear;
			print "Directly updating $pkgname -> ", $l->[0]->name, "\n";
			$self->add_updateset($set, $h, $l->[0]);
			return 1;
		}
		if (defined $found && $found eq $l->[0] && 
		    !$plist->uses_old_libs && !$state->{defines}->{installed}) {
				my $msg = "No need to update $pkgname";
				$state->progress->message($msg);
				print "$msg\n" if $state->{beverbose};
				return 0;
		}
	}

	$state->progress->clear;
	my %cnd = map {($_->name, $_)} @$l;
	print "Candidates for updating $pkgname -> ", join(' ', keys %cnd), "\n";
		
	if (@$l == 1) {
		$self->add_updateset($set, $h, $l->[0]);
		return 1;
	}
	my $result = OpenBSD::Interactive::choose1($pkgname, 
	    $state->{interactive}, sort keys %cnd);
	if (defined $result) {
		if (defined $found && $found eq $result && 
		    !$plist->uses_old_libs) {
			print "No need to update $pkgname\n";
			return 0;
		} else {
			$self->add_updateset($set, $h, $cnd{$result});
			return 1;
		}
	} else {
		$state->{issues} = 1;
		return undef;
	}
}

1;
