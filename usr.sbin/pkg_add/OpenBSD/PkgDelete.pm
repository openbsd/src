#!/usr/bin/perl
# ex:ts=8 sw=4:
# $OpenBSD: PkgDelete.pm,v 1.24 2011/08/26 08:46:10 espie Exp $
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

use OpenBSD::AddDelete;

package OpenBSD::PkgDelete::Tracker;

sub new
{
	my $class = shift;
	bless {}, $class;
}

sub sets_todo
{
	my ($self, $offset) = @_;
	return sprintf("%u/%u", (scalar keys %{$self->{done}})-$offset,
		scalar keys %{$self->{total}});
}

sub handle_set
{
	my ($self, $set) = @_;
	$self->{total}{$set} = 1;
	if ($set->{finished}) {
		$self->{done}{$set} = 1;
	}
}

sub todo
{
	my ($self, @list) = @_;
	for my $set (@list) {
		for my $pkgname ($set->older_names) {
			$self->{todo}{$pkgname} = $set;
		}
		$self->handle_set($set);
	}
}


sub done
{
	my ($self, $set) = @_;
	$set->{finished} = 1;
	for my $pkgname ($set->older_names) {
		delete $self->{todo}{$pkgname};
	}
	$self->handle_set($set);
}

sub cant
{
	&done;
}
sub find
{
	my ($self, $pkgname) = @_;
	return $self->{todo}{$pkgname};
}



package OpenBSD::PkgDelete::State;
our @ISA = qw(OpenBSD::AddDelete::State);

sub new
{
	my $class = shift;
	my $self = $class->SUPER::new(@_);
	$self->{tracker} = OpenBSD::PkgDelete::Tracker->new;
	return $self;
}

sub tracker
{
	my $self = shift;
	return $self->{tracker};
}

sub handle_options
{
	my $state = shift;
	$state->SUPER::handle_options('',
	    '[-acinqsvx] [-B pkg-destdir] [-D name[=value]] pkg-name [...]');

	my $base = $state->opt('B') // $ENV{'PKG_DESTDIR'} // '';
	if ($base ne '') {
		$base.='/' unless $base =~ m/\/$/o;
	}
	$ENV{'PKG_DESTDIR'} = $base;

	$state->{destdir} = $base;
	if ($base eq '') {
	    $state->{destdirname} = '';
	} else {
	    $state->{destdirname} = '${PKG_DESTDIR}';
	}
}

sub stem2location
{
	my ($self, $locator, $name, $state) = @_;
	require OpenBSD::Search;
	my $l = $locator->match_locations(OpenBSD::Search::Stem->new($name));
	if (@$l > 1 && !$state->defines('allversions')) {
		$l = OpenBSD::Search::FilterLocation->keep_most_recent->filter_locations($l);
	}
	return $state->choose_location($name, $l);
}

sub deleteset
{
	my $self = shift;
	require OpenBSD::UpdateSet;

	return OpenBSD::DeleteSet->new($self);
}

sub deleteset_from_location
{
	my ($self, $location) = @_;
	return $self->deleteset->add_older(OpenBSD::Handle->from_location($location));
}

package OpenBSD::DeleteSet;
sub setup_header
{
	my ($set, $state, $handle) = @_;
	my $header = $state->deptree_header($set);
	if (defined $handle) {
		$header .= $handle->pkgname;
	} else {
		$header .= $set->print;
	}
	if (!$state->progress->set_header($header)) {
		return unless $state->verbose;
		$header = "Deleting $header";
		if (defined $state->{lastheader} &&
		    $header eq $state->{lastheader}) {
			return;
		}
		$state->{lastheader} = $header;
		$state->print("#1", $header);
		$state->print("(pretending) ") if $state->{not};
		if ($state->{do_faked}) {
			$state->print(" under #1", $state->{destdir});
		}
		$state->print("\n");
	}
}

package OpenBSD::PkgDelete;
our @ISA = qw(OpenBSD::AddDelete);

use OpenBSD::PackingList;
use OpenBSD::RequiredBy;
use OpenBSD::Delete;
use OpenBSD::PackageInfo;
use OpenBSD::UpdateSet;
use OpenBSD::Handle;


sub add_location
{
	my ($self, $state, $l) = @_;
	push(@{$state->{setlist}},
	    $state->deleteset_from_location($l));
}

sub create_locations
{
	my ($state, @l) = @_;
	my $inst = $state->repo->installed;
	my $result = [];
	for my $name (@l) {
		my $l = $inst->find($name, $state->{arch});
		if (!defined $l) {
			$state->errsay("Can't find #1 in installed packages",
			    $name);
			$state->{bad}++;
		} else {
			push(@$result, $state->deleteset_from_location($l));
		}
	}
	return $result;
}

sub process_parameters
{
	my ($self, $state) = @_;

	my $inst = $state->repo->installed;

	if (@ARGV == 0) {
		if (!$state->{automatic}) {
			$state->fatal("No packages to delete");
		}
	} else {
		for my $pkgname (@ARGV) {
			my $l;

			if (OpenBSD::PackageName::is_stem($pkgname)) {
				$l = $state->stem2location($inst, $pkgname,
				    $state);
			} else {
				$l = $inst->find($pkgname);
			}
			if (!defined $l) {
				$state->say("Problem finding #1", $pkgname);
				$state->{bad}++;
			} else {
				$self->add_location($state, $l);
			}
		}
	}
}

sub finish_display
{
}

sub really_remove
{
	my ($set, $state) = @_;
	if ($state->{not}) {
		$state->status->what("Pretending to delete");
	} else {
		$state->status->what("Deleting");
	}
	$set->setup_header($state);
	for my $pkgname ($set->older_names) {
		$set->setup_header($state, $set->{older}{$pkgname});
		$state->log->set_context('-'.$pkgname);
		OpenBSD::Delete::delete_package($pkgname, $state);
	}
	$state->progress->next($state->ntogo);
	$state->syslog("Removed #1", $set->print);
}

sub delete_dependencies
{
	my $state = shift;

	if ($state->defines("dependencies")) {
		return 1;
	}
	if ($state->{interactive}) {
		return $state->confirm("Delete them as well", 0);
	}
	return 0;
}

sub fix_bad_dependencies
{
	my $state = shift;

	if ($state->defines("baddepend")) {
		return 1;
	}
	if ($state->{interactive}) {
		return $state->confirm("Delete anyways", 0);
	}
	return 0;
}

sub process_set
{
	my ($self, $set, $state) = @_;

	my $todo = {};
	my $bad = {};
    	for my $pkgname ($set->older_names) {
		unless (is_installed($pkgname)) {
			$state->errsay("#1 was not installed", $pkgname);
			$set->{finished} = 1;
			$set->cleanup(OpenBSD::Handle::NOT_FOUND);
			$state->{bad}++;
			return ();
		}
		my $r = OpenBSD::RequiredBy->new($pkgname);
		for my $pkg ($r->list) {
			next if $set->{older}->{$pkg};
			my $f = $state->tracker->find($pkg);
			if (defined $f) {
				$todo->{$pkg} = $f;
			} else {
				$bad->{$pkg} = 1;
			}
		}
	}
	if (keys %$bad > 0) {
		my $bad2 = {};
		for my $pkg (keys %$bad) {
			if (!is_installed($pkg)) {
				$bad2->{$pkg} = 1;
			}
		}
		if (keys %$bad2 > 0) {
			$state->errsay("#1 depends on non-existant #2",
			    $set->print, join(' ', sort keys %$bad2));
			if (fix_bad_dependencies($state)) {
				for my $pkg (keys %$bad2) {
					delete $bad->{$pkg};
				}
			}
		}
	}
	if (keys %$bad > 0) {
		if (!$state->{do_automatic} || $state->verbose) {
			$state->errsay("can't delete #1 without deleting #2",
			    $set->print, join(' ', sort keys %$bad));
		}
		if (!$state->{do_automatic}) {
			if (delete_dependencies($state)) {
			    	my $l = create_locations($state, keys %$bad);
				$state->tracker->todo(@$l);
				return (@$l, $set);
			}
			$state->{bad}++;
	    	}
		$set->cleanup(OpenBSD::Handle::CANT_DELETE);
		$state->tracker->cant($set);
		return ();
	}
	# XXX this si where we should detect loops
	if (keys %$todo > 0) {
		if ($set->{once}) {
			for my $set2 (values %$todo) {
				# XXX merge all ?
				$set->add_older($set2->older);
				$set2->{merged} = $set;
				$set2->{finished} = 1;
			}
			delete $set->{once};
			return ($set);
		}
		$set->{once} = 1;
		$state->build_deptree($set, values %$todo);
		return (values %$todo, $set);
	}
	if ($state->{do_automatic}) {
		for my $pkg  ($set->older) {
			$pkg->complete_old;
			if ($pkg->plist->has('manual-installation')) {
				$state->say("Won't delete manually installed #1",
				    $set->print) if $state->verbose;
				$set->cleanup(OpenBSD::Handle::CANT_DELETE);
				$state->tracker->cant($set);
				return ();
			}
		}
	}
	really_remove($set, $state);
	$set->cleanup;
	$state->tracker->done($set);
	return ();
}

sub main
{
	my ($self, $state) = @_;

	$self->process_setlist($state);
	if ($state->{automatic}) {
		my $inst = $state->repo->installed;
		delete $state->{setlist};
		for my $l (@{$inst->locations_list}) {
			$self->add_location($state, $l);
		}
		$state->{do_automatic} = 1;
		$self->process_setlist($state);
	}
}

sub new_state
{
	my ($self, $cmd) = @_;
	return OpenBSD::PkgDelete::State->new($cmd);
}

1;
