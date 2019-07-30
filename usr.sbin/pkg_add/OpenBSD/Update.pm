# ex:ts=8 sw=4:
# $OpenBSD: Update.pm,v 1.162 2015/01/04 14:20:04 espie Exp $
#
# Copyright (c) 2004-2014 Marc Espie <espie@openbsd.org>
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
	my ($self, $state, @r) = @_;
	my $msg = $state->f(@r);
	$msg .= $state->ntogo_string;
	$state->progress->message($msg);
	$state->say($msg) if $state->verbose >= 2;
}

sub process_handle
{
	my ($self, $set, $h, $state) = @_;
	my $pkgname = $h->pkgname;

	if ($pkgname =~ m/^\.libs\d*\-/o) {
		return 0;
	}

	if (!$set->{quirks}) {
		my $base = 0;
		$state->run_quirks(
		    sub {
			my $quirks = shift;
			$base = $quirks->is_base_system($h, $state);
		    });
		if ($base) {
			$h->{update_found} = OpenBSD::Handle->system;
			$set->{updates}++;
			return 1;
		}
	}

	my $plist = OpenBSD::PackingList->from_installation($pkgname,
	    \&OpenBSD::PackingList::UpdateInfoOnly);
	if (!defined $plist) {
		$state->fatal("can't locate #1", $pkgname);
	}

	if ($plist->has('firmware') && !$state->defines('FW_UPDATE')) {
		$set->move_kept($h);
		return 0;
	}

#	if (defined $plist->{url}) {
#		my $repo;
#		($repo, undef) = $state->repo->path_parse($plist->{url}->name);
#		$set->add_repositories($repo);
#	}
	my @search = ();

	my $sname = $pkgname;
	while ($sname =~ s/^partial\-//o) {
	}
	push(@search, OpenBSD::Search::Stem->split($sname));

	if (!$set->{quirks}) {
		$state->run_quirks(
		    sub {
			my $quirks = shift;
			$quirks->tweak_search(\@search, $h, $state);
		    });
	}
	my $oldfound = 0;
	my @skipped_locs = ();

	# XXX this is nasty: maybe we added an old set to update
	# because of conflicts, in which case the pkgpath +
	# conflict should be enough  to "match".
	for my $n ($set->newer) {
		if (($state->{hard_replace} ||
		    $n->location->update_info->match_pkgpath($plist)) &&
			$n->conflict_list->conflicts_with($sname)) {
				$self->add_handle($set, $h, $n);
				return 1;
		}
	}
	if (!$state->defines('downgrade')) {
		push(@search, OpenBSD::Search::FilterLocation->more_recent_than($sname, \$oldfound));
	}
	push(@search, OpenBSD::Search::FilterLocation->new(
	    sub {
		my $l = shift;
		if (@$l == 0) {
			return $l;
		}
		my @l2 = ();
		for my $loc (@$l) {
		    if (!$loc) {
			    next;
		    }
		    my $p2 = $loc->update_info;
		    if (!$p2) {
			next;
		    }
		    if ($p2->has('arch')) {
			unless ($p2->{arch}->check($state->{arch})) {
			    $loc->forget;
			    next;
			}
		    }
		    if (!$plist->match_pkgpath($p2)) {
			push(@skipped_locs, $loc);
			next
		    }
		    my $r = $plist->signature->compare($p2->signature);
		    if (defined $r && $r > 0 && !$state->defines('downgrade')) {
		    	$oldfound = 1;
			$loc->forget;
			next;
		    }
		    push(@l2, $loc);
		}
		return \@l2;
	    }));

	if (!$state->defines('allversions')) {
		push(@search, OpenBSD::Search::FilterLocation->keep_most_recent);
	}

	my $l = $set->match_locations(@search);

	for my $loc (@skipped_locs) {
		if (@$l == 0 && $state->verbose) {
			$self->say_skipped_packages($state, $plist,
				$loc->update_info);
		}
		$loc->forget;
	}

	if (@$l == 0) {
		if ($oldfound) {
			$set->move_kept($h);
			$self->progress_message($state,
			    "No need to update #1", $pkgname);

			return 0;
		}
		return undef;
	}
	$state->say("Update candidates: #1 -> #2#3", $pkgname,
	    join(' ', map {$_->name} @$l), $state->ntogo_string) 
		if $state->verbose;

	my $r = $state->choose_location($pkgname, $l);
	if (defined $r) {
		$self->add_location($set, $h, $r);
		return 1;
	} else {
		$state->{issues} = 1;
		return undef;
	}
}

sub say_skipped_packages
{
	my ($self, $state, $o, $n) = @_;

	my $o_name = $o->pkgname;
	my @o_ps = map { @{$o->pkgpath->{$_}} } keys %{$o->pkgpath};
	my $o_pp = join(" ", map {$_->fullpkgpath} @o_ps);

	my $n_name = $n->pkgname;
	my @n_ps = map { @{$n->pkgpath->{$_}} } keys %{$n->pkgpath};
	my $n_pp= join(" ", map {$_->fullpkgpath} @n_ps);

	my $t  = "Skipping #1 (update candidate for #2)";
	   $t .= "\n\t#2 pkgpaths: #4\n\t#1 pkgpaths: #3";

	$state->say($t, $n_name, $o_name, $n_pp, $o_pp);
}

sub find_nearest
{
	my ($base, $locs) = @_;

	my $pkgname = OpenBSD::PackageName->from_string($base);
	return undef if !defined $pkgname->{version};
	my @sorted = sort {$a->pkgname->{version}->compare($b->pkgname->{version}) } @$locs;
	if ($sorted[0]->pkgname->{version}->compare($pkgname->{version}) > 0) {
		return $sorted[0];
	}
	if ($sorted[-1]->pkgname->{version}->compare($pkgname->{version}) < 0) {
		return $sorted[-1];
	}
	return undef;
}

sub process_hint
{
	my ($self, $set, $hint, $state) = @_;

	my $l;
	my $hint_name = $hint->pkgname;
	my $k = OpenBSD::Search::FilterLocation->keep_most_recent;
	# first try to find us exactly

	$self->progress_message($state, "Looking for #1", $hint_name);
	$l = $set->match_locations(OpenBSD::Search::Exact->new($hint_name), $k);
	if (@$l == 0) {
		my $t = $hint_name;
		$t =~ s/\-\d([^-]*)\-?/--/;
		my @search = (OpenBSD::Search::Stem->new($t));
		$state->run_quirks(
		    sub {
			my $quirks = shift;
			$quirks->tweak_search(\@search, $hint, $state);
		    });
		$l = $set->match_locations(@search, $k);
	}
	if (@$l > 1) {
		my $r = find_nearest($hint_name, $l);
		if (defined $r) {
			$self->add_location($set, $hint, $r);
			return 1;
		}
	}
	my $r = $state->choose_location($hint_name, $l);
	if (defined $r) {
		$self->add_location($set, $hint, $r);
		OpenBSD::Add::tag_user_packages($set);
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
		if ($pkgname =~ m/[\/\:]/o) {
			my $repo;
			($repo, $pkgname) = $state->repo->path_parse($pkgname);
			$set->add_repositories($repo);
		};
		my $l = $state->updater->stem2location($set, $pkgname, $state,
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
			$state->say("Can't update #1: no update found for #2",
			    $set->print, join(',', @problems));
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
	my ($self, $locator, $name, $state, $is_quirks) = @_;
	my $l = $locator->match_locations(OpenBSD::Search::Stem->new($name));
	if (@$l > 1 && !$state->defines('allversions')) {
		$l = OpenBSD::Search::FilterLocation->keep_most_recent->filter_locations($l);
	}
	return $state->choose_location($name, $l, $is_quirks);
}

1;
