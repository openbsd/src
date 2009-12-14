# ex:ts=8 sw=4:
# $OpenBSD: Update.pm,v 1.117 2009/12/14 11:19:04 espie Exp $
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

use strict;
use warnings;

package OpenBSD::Handle;
sub update
{
	my ($self, $updater, $set, $state) = @_;

	return $updater->process_handle($set, $self, $state);
}

package OpenBSD::hint;
sub update
{
	my ($self, $updater, $set, $state) = @_;

	return $updater->process_hint($set, $self, $state);
}

package OpenBSD::hint2;
sub update
{
	my ($self, $updater, $set, $state) = @_;

	return $updater->process_hint2($set, $self, $state);
}

package OpenBSD::Update;
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

sub add_handle
{
	my ($self, $set, $old, $n) = @_;
	$old->{update_found} = $n;
	$set->add_newer($n);
}

sub add_location
{
	my ($self, $set, $handle, $location) = @_;

	$self->add_handle($set, $handle, 
	    OpenBSD::Handle->from_location($location));
}

sub progress_message
{
	my ($self, $state, $msg) = @_;
	$msg .= $state->ntogo;
	$state->progress->message($msg);
	$state->say($msg) if $state->{beverbose};
}

my $first = 1;
sub process_handle
{
	my ($self, $set, $h, $state) = @_;
	my $pkgname = $h->pkgname;

	if ($pkgname =~ m/^\.libs\d*\-/o) {
		if ($first) {
			$state->say("Not updating .libs*, remember to clean them");
			$first = 0;
		}
		return 0;
	}
	if ($pkgname =~ m/^partial\-/o) {
		$state->say("Not updating $pkgname, remember to clean it");
		$h->{keepit} = 1;
		return 0;
	}


	eval {
		if ($state->quirks->is_base_system($h, $state)) {
			$h->{update_found} = 1;
			$set->{updates}++;
		}
	};
	return 1 if $h->{update_found};

	my $plist = OpenBSD::PackingList->from_installation($pkgname, 
	    \&OpenBSD::PackingList::UpdateInfoOnly);
	if (!defined $plist) {
		Fatal("Can't locate $pkgname");
	}

	my @search = ();
	push(@search, OpenBSD::Search::Stem->split($pkgname));

	eval {
		$state->quirks->tweak_search(\@search, $h, $state);
	};
	my $found;
	my $oldfound = 0;

	# XXX this is nasty: maybe we added an old set to update 
	# because of conflicts, in which case the pkgpath + 
	# conflict should be enough  to "match".
	for my $n ($set->newer) {
		if ($n->location->update_info->match_pkgpath($plist) &&
			$n->plist->conflict_list->conflicts_with($pkgname)) {
				$self->add_handle($set, $h, $n);
				return 1;
		}
	}
	if (!$state->{defines}->{downgrade}) {
		push(@search, OpenBSD::Search::FilterLocation->more_recent_than($pkgname, \$oldfound));
	}
	push(@search, OpenBSD::Search::FilterLocation->new(
	    sub {
		my $l = shift;
		if (@$l == 0) {
			return $l;
		}
		my @l2 = ();
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
		if ($oldfound) {
			$h->{update_found} = $h;
			$h->{keepit} = 1;

			$self->progress_message($state, 
			    "No need to update $pkgname");
			
			return 0;
		}
		return undef;
	}
	if (@$l == 1) {
		if (defined $found && $found eq $l->[0] &&
		    !$plist->uses_old_libs && !$state->{defines}->{installed}) {
			$h->{update_found} = $h;
			$h->{keepit} = 1;

			$self->progress_message($state, 
			    "No need to update $pkgname");

			return 0;
		}
	}

	$state->say("Update candidates: $pkgname -> ", 
	    join(' ', map {$_->name} @$l), $state->ntogo);
		
	my $r = $state->choose_location($pkgname, $l);
	if (defined $r) {
		$self->add_location($set, $h, $r);
		return 1;
	} else {
		$state->{issues} = 1;
		return undef;
	}
}

sub process_hint
{
	my ($self, $set, $hint, $state) = @_;

	my $l;
	my $hint_name = $hint->pkgname;
	my $k = OpenBSD::Search::FilterLocation->keep_most_recent;
	# first try to find us exactly

	$state->progress->message("Looking for $hint_name");
	$l = OpenBSD::PackageLocator->match_locations(OpenBSD::Search::Exact->new($hint_name), $k);
	if (@$l == 0) {
		my $t = $hint_name;
		$t =~ s/\-\d([^-]*)\-?/--/;
		$l = OpenBSD::PackageLocator->match_locations(OpenBSD::Search::Stem->new($t), $k);
	}
	my $r = $state->choose_location($hint_name, $l);
	if (defined $r) {
		$self->add_location($set, $hint, $r);
		return 1;
	} else {
		return 0;
	}
}

my $cache = {};

sub process_hint2
{
	my ($self, $set, $hint, $state) = @_;
	my $pkgname = $hint->pkgname;
	if (OpenBSD::PackageName::is_stem($pkgname)) {
		my ($h, $path, $repo);
		if ($pkgname =~ m/\//o) {
			($repo, $path, $pkgname) = OpenBSD::PackageLocator::path_parse($pkgname);
			$h = $repo;
		} else {
			$h = 'OpenBSD::PackageLocator';
			$path = "";
		}
		my $l = $state->updater->stem2location($h, $pkgname, $state, 
		    $set->{quirks});
		if (defined $l) {
			$self->add_location($set, $hint, $l);
		} else {
			return undef;
		}
	} else {
		if (!defined $cache->{$pkgname}) {
			$self->add_handle($set, $hint, OpenBSD::Handle->create_new($pkgname));
			$cache->{$pkgname} = 1;
		}
	}
	OpenBSD::Add::tag_user_packages($set);
	return 1;
}

sub process_set
{
	my ($self, $set, $state) = @_;
	my @problems = ();
	for my $h ($set->older, $set->hints) {
		next if $h->{update_found};
		if (!defined $h->update($self, $set, $state)) {
			push(@problems, $h->pkgname);
		}
	}
	if (@problems > 0) {
		$state->tracker->cant($set) if !$set->{quirks};
		if ($set->{updates} != 0) {
			$state->say("Can't update ", $set->print, 
			    ": no update found for ", 
			    join(',', @problems));
		}
		return 0;
	} elsif ($set->{updates} == 0) {
		$state->tracker->uptodate($set);
		return 0;
	} 
	$state->tracker->add_set($set);
	return 1;
}

sub stem2location
{
	my ($self, $repo, $name, $state, $is_quirks) = @_;
	my $l = $repo->match_locations(OpenBSD::Search::Stem->new($name));
	if (@$l > 1 && !$state->{defines}->{allversions}) {
		$l = OpenBSD::Search::FilterLocation->keep_most_recent->filter_locations($l);
	}
	return $state->choose_location($name, $l, $is_quirks);
}
 
1;
